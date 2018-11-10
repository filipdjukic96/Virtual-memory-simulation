#include "thrashingmanager.h"
#include "vmallocator.h"
#include "stdmutexlocker.h"
#include "kernelsystem.h"
#include "kernelprocess.h"


using namespace std;




ThrashingManager::ThrashingManager() {
	//period na svakih koliko periodicJob se obradjuje thrashing
	counterForThrashingProcessing = 50;
}



void ThrashingManager::splitProcessesForThrashing(std::vector<KernelProcess*>& active, std::vector<KernelProcess*>& thrashing) {
	for (auto it = KernelSystem::instance->processMap.begin(); it != KernelSystem::instance->processMap.end(); ++it) {
		//prolaz kroz sve procese sistema i na osnovu isInThrashing flega se rasporedjuju
		KernelProcess* process = it->second;
		if (!process->isInThrashing) {
			active.push_back(process);
		}
		else {
			thrashing.push_back(process);
		}
	}
}


//ukupni radni skup svih procesa
PageNum ThrashingManager::calcTotalNeededSize(const std::vector<KernelProcess*>& active) {
	PageNum sum = 0;
	for (auto it = active.begin(); it != active.end(); ++it) {
		KernelProcess* process = *it;
		StdRecursiveMutexLocker locker(process->kpMutex);
		sum += process->accessHistory.size();
	}
	return sum;
}


//obrada thrashing-a
void ThrashingManager::processThrashing() {
	counterForThrashingProcessing--;
	
	if (counterForThrashingProcessing == 0) {
		counterForThrashingProcessing = 50;

		//vektori procesa koji su u thrashing-u i koji nisu(aktivni)-->trenutno
		vector<KernelProcess*> thrashingProcesses;
		vector<KernelProcess*> activeProcesses;
		
		//podjela procesa na thrashing i aktivne
		splitProcessesForThrashing(activeProcesses, thrashingProcesses);

		//ukupan radni skup svih procesa
		PageNum neededSize = calcTotalNeededSize(activeProcesses);

		//ima vise frame-ova u OM od ukupnog potrebnog radnog skupa
		if (neededSize <= KernelSystem::instance->frameManager->vmAllocator->getSizeInFrames()) {
			//prolazak kroz thrashing procese
			for (auto it = thrashingProcesses.begin(); it != thrashingProcesses.end(); ++it) {
				KernelProcess* process = *it;
				PageNum accessHistorySize = 0;
				//uzimanje radnog skupa thrashing procesa
				{
					StdRecursiveMutexLocker locker(process->kpMutex);
					accessHistorySize = process->accessHistory.size();
				}
				
				PageNum newNeededSize = neededSize + accessHistorySize;

				if (newNeededSize <= KernelSystem::instance->frameManager->vmAllocator->getSizeInFrames()) {
					//ako ima dovoljno frame-ova za radni skup thrashing procesa
					//on se stavlja u active stanje i obavjestava se njegova condition varijabla
					unique_lock<mutex> lck(process->thrashingMutex);
					if (process->isInThrashing == true) {
						process->isInThrashing = false;
						process->condVarThrashingBlock.notify_one();
					}
					neededSize = newNeededSize;
				}
			}
		}
		//u OM nema dovoljno frame-ova za ukupan radni skup svih aktivnih procesa
		else {
			//prolazak kroz active procese
			for (auto it = activeProcesses.begin(); it != activeProcesses.end(); ++it) {
				KernelProcess* process = *it;
				StdRecursiveMutexLocker locker(process->kpMutex);
				//proces se stavlja u thrashing stanje
				process->isInThrashing = true;
				//ukupan radni skup se umanjuje za radni skup ovog procesa
				neededSize -= process->accessHistory.size();
				//ako je blokiran dovoljan broj procesa
				if (neededSize <= KernelSystem::instance->frameManager->vmAllocator->getSizeInFrames()) {
					break;
				}
			}
		}
		
	
		for (auto it = activeProcesses.begin(); it != activeProcesses.end(); ++it) {
			KernelProcess* process = *it;
			StdRecursiveMutexLocker locker(process->kpMutex);
			if (process->isInThrashing == false) {
				process->accessHistory = unordered_set<PageNum>();
			}
		}

		
		for (auto it = thrashingProcesses.begin(); it != thrashingProcesses.end(); ++it) {
			KernelProcess* process = *it;
			StdRecursiveMutexLocker locker(process->kpMutex);
			if (process->isInThrashing == false) {
				process->accessHistory = unordered_set<PageNum>();
			}
		}
	}
}
