#include "kernelsystem.h"
#include "kernelprocess.h"
#include "framemanager.h"
#include <math.h>
#include "stdmutexlocker.h"
#include <iostream>
#include "pmtallocator.h"
#include "pmt.h"
#include "process.h"
#include "sharedsegment.h"
#include "thrashingmanager.h"

using namespace std;




KernelSystem* KernelSystem::instance = nullptr;


KernelSystem::KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition * partition) {
	if (instance != nullptr) {
		cout << "PMTAllocator::FreeFrames - frameAdr nije poravnata na okvir!" << endl;
		throw std::exception();
	}
	//kreirenje instance kernelSystem,alociranje frameManager-a i pmtAllocator-a
	instance = this;
	spaceAllocator = new PMTAllocator(pmtSpace, pmtSpaceSize);
	frameManager = new FrameManager(processVMSpace, processVMSpaceSize, partition);
	nextId = 1;


	sharedSegmentsManager = new SharedSegmentsManager();
	thrashingManager = new ThrashingManager();
}


KernelSystem::~KernelSystem() {
	delete thrashingManager;
	delete sharedSegmentsManager;
	delete frameManager;
	delete spaceAllocator;
}


Process * KernelSystem::createProcess() {
	StdRecursiveMutexLocker locker(ksMutex);
	ProcessId pid = nextId++;
	//u slucaju da je ovaj pid vec zauzet
	while (processMap.find(pid) != processMap.end()) {
		pid = nextId++;
	}

	return new Process(pid);
}


//fja za kreiranje kernelProcess-a
KernelProcess * KernelSystem::createKernelProcess(ProcessId pid, Process* process) {
	StdRecursiveMutexLocker locker(ksMutex);
	//alociranje okvira za pmt1
	PMT1Descriptor* pmt1 = (PMT1Descriptor*)spaceAllocator->allocateFrame();
	KernelProcess* proc = nullptr;
	if (pmt1 != nullptr) {
		proc = new KernelProcess(pid, process, pmt1);
	}


	return proc;
}


void KernelSystem::destroyKernelProcess(KernelProcess* kProcess) {
	if (kProcess == nullptr) {
		return;
	}

	StdRecursiveMutexLocker locker(ksMutex);
	delete kProcess;
}


Time KernelSystem::periodicJob() {
	StdRecursiveMutexLocker locker(ksMutex);
	//nema procesa u kernelSystemu
	if (!processMap.size()) {
		return 0;
	}

	thrashingManager->processThrashing();
	return 10000 + rand() % 10000;
}


//fja za kloniranje procesa sa navedenim pid-om
Process * KernelSystem::cloneProcess(ProcessId pid) {
	StdRecursiveMutexLocker locker(ksMutex);
	KernelProcess* kProcess = nullptr;

	//trazenje procesa sa navedenim pid-om
	auto it = processMap.find(pid);
	if (it == processMap.end()) {
		return nullptr;
	}

	kProcess = it->second;

	//novi id koji ce da ima klonirani proces
	ProcessId clonePid = nextId++;

	//ukoliko je pid zauzet,tj vec postoji proces sa tim pid-om
	while (processMap.find(clonePid) != processMap.end()) {
		clonePid = nextId++;
	}

	//poziv clone od klase kernelProcess,tj njenog Process-a,da se klonira
	Process* proc = kProcess->myProcess->clone(clonePid);

	return proc;
}

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType type) {
	StdRecursiveMutexLocker locker(ksMutex);
	auto it = processMap.find(pid);
	if (it == processMap.end()) {//nije pronadjen proces
		return Status::TRAP;
	}
	KernelProcess* process = it->second;
	if (process == nullptr) {
		return Status::TRAP;
	}

	Status result = process->access(address, type);
	return result;
}
