#include <Windows.h>

#include "kernelprocess.h"
#include "stdmutexlocker.h"
#include "process.h"
#include "pmtallocator.h"
#include "utility.h"
#include "kernelsystem.h"
#include <iostream>
#include <vector>
#include "framemanager.h"
#include "vmallocator.h"
#include "splitter.h"
#include "thrashingmanager.h"

using namespace std;




KernelProcess::KernelProcess(ProcessId pid, Process* myProcess, PMT1Descriptor* pmt1) {
	StdRecursiveMutexLocker locker(kpMutex);

	//preuzima dodijeljeni pid
	myPid = pid;
	//bitset tj niz bool vrijednosti koje oznacavaju zauzete stranice
	//ovog procesa,setuje ih sve na 1
	segmentAllocatedPages.set();

	this->myProcess = myProcess;
	this->pmt1 = pmt1;
	isInThrashing = false;//proces nije u thrashing-u

	auto it = KernelSystem::instance->processMap.find(myPid);
	if (it != KernelSystem::instance->processMap.end()) {
		cout << "greska, vec postoji proces sa id " << pid << endl;
		std::exception();
	}

	//ubacuje sebe u mapu kernelProcessa sistema(po pid-u)
	KernelSystem::instance->processMap[myPid] = this;
}

KernelProcess::~KernelProcess() {
	StdRecursiveMutexLocker locker(kpMutex);
	
	//dok god proces ima alociranih segmenata
	while (segments.empty() == false) {

		//pronalazi prvi segment
		auto it = segments.begin();
		Segment segment = it->second;
		//VA segmenta
		VirtualAddress address = Splitter::virtualAddressFromPage(segment.getStartPage());
		
		if (segment.getIsShared()) {
			//ako je dijeljen segment,proces se odjavljuje sa njega
			string name = virtualAddressSharedSegmentMap[address];
			disconnectSharedSegment(name.c_str());
		}
		else {
			//u suprotnom,segment je privatni,pa proces ga brise
			deleteSegmentPrivate(address);
		}
	}

	KernelSystem::instance->thrashingManager->counterForThrashingProcessing = 50; // da pozuri azuriranje za ostale
	//brisanje pmt1
	KernelSystem::instance->spaceAllocator->freeFrame((PhysicalAddress*)pmt1);

	//zakljucavanje mutex-a kernelSystem-a jer se proces uklanja iz njega
	StdRecursiveMutexLocker systemLocker(KernelSystem::instance->ksMutex);
	//brise se ovaj proces iz hesmape procesa po pid-u u sistemu
	KernelSystem::instance->processMap.erase(myPid);
}

ProcessId KernelProcess::getProcessId() const {
	return myPid;
}

Status KernelProcess::access(VirtualAddress address, AccessType type) {
	
	if (PMT::checkVirtualAddressBounds(address) == false) {
		return Status::TRAP;
	}

	//postavlja posljednju pristupanu Va i accessType koji je trazen
	lastAccessData.virtualAddress = address;
	lastAccessData.accessType = type;

	PageNum page1, page2, offset;
	Splitter::splitVirtualAddress(address, page1, page2, offset);

	
	if (pmt1[page1].pmt2Valid == 0) {
		return Status::TRAP;
	}

	PMT2Descriptor* pmt2 = (PMT2Descriptor*)KernelSystem::instance->spaceAllocator->getPhysicalAddressFromFrameNum(pmt1[page1].pmt2FrameNum);
	PMT2Descriptor* pmt2Desc = &pmt2[page2];

	
	if (pmt2Desc->vmValid == 0) {
		return Status::PAGE_FAULT;
	}

	Status accessStatus = isAccessAllowed(pmt2Desc, type);
	if (accessStatus != Status::OK) {
		return accessStatus;
	}

	
	if (type == AccessType::WRITE || type == AccessType::READ_WRITE) {
		pmt2Desc->dirty = 1;
	}

	
	pmt2Desc->referenceBit = 1;

	PageNum page = Splitter::pageFromPage1AndPage2(page1, page2);

	StdRecursiveMutexLocker locker(kpMutex);
	
	accessHistory.emplace(page);

	return Status::OK;
}

Status KernelProcess::pageFault(VirtualAddress address) {
	if (PMT::checkVirtualAddressBounds(address) == false) {
		return Status::TRAP;
	}

	PageNum page = Splitter::pageFromVA(address);
	PageNum page1, page2, offset;
	Splitter::splitVirtualAddress(address, page1, page2, offset);

	if (pmt1[page1].pmt2Valid == 0) {
		return Status::TRAP;
	}
	else if (segmentAllocatedPages[page] == true) {
		
		return Status::TRAP;
	}
	else {
		PMT2Descriptor* pmt2 = (PMT2Descriptor*)KernelSystem::instance->spaceAllocator->getPhysicalAddressFromFrameNum(pmt1[page1].pmt2FrameNum);
		PMT2Descriptor* pmt2Desc = &pmt2[page2];

	
		PhysicalAddress frameAdr = KernelSystem::instance->frameManager->loadFrameInVM(pmt2Desc);
		if (frameAdr == nullptr) {
			return Status::PAGE_FAULT;
		}

		//provjera da li je dozvoljen posljednji zatrazen pristup
		//u slucaju da treba da se obradi COW
		Status canAccess = isAccessAllowed(pmt2Desc, lastAccessData.accessType);
		if (canAccess != Status::OK){
				//pristup nije dozvoljen,provjerava se da li je potrebno
				//obraditi COW(da li je COW u pitanju)
			if (shouldHandleCOW(pmt2Desc) != Status::OK) {
				return Status::PAGE_FAULT;
			}


		
			unsigned char* buffer = new unsigned char[PMT::PageSize];
	
			memcpy(buffer, frameAdr, PMT::PageSize);


			FrameDescriptor* parentFrameDesc = KernelSystem::instance->frameManager->frameDescFromPMT2Desc(pmt2Desc);

	
			bool originalParentCopyOnWrite = parentFrameDesc->ownerCount() > 1;

		
			parentFrameDesc->remove(pmt2Desc);

		
			if (parentFrameDesc->ownerCount() == 1) {
				parentFrameDesc->resetCopyOnWrite();
			}

			
			FrameDescriptor* childFrameDesc = new FrameDescriptor();

			//backup pmt2Desc
			PMT2Descriptor pmt2DescriptorBackup = *pmt2Desc;

		
			pmt2Desc->vmValid = 0;
			pmt2Desc->vmFrameNum = 0;
			pmt2Desc->diskValid = 0;
			pmt2Desc->diskFrameNum = (PageNum)childFrameDesc;


			childFrameDesc->put(pmt2Desc);
			childFrameDesc->resetCopyOnWrite();

		
			frameAdr = KernelSystem::instance->frameManager->loadFrameInVM(pmt2Desc);
			
			//uspjesno alociran frame za pmt2Desc
			if (frameAdr != nullptr) {
				//prepisuje se sadrzaj u novi okvir iz dijeljene stranice
				memcpy(frameAdr, buffer, PMT::PageSize);

				delete[] buffer;
				return Status::OK;
			}
			else {//nema mjesta za novi okvir u OM,rollback

				delete[] buffer;
			
				childFrameDesc->remove(pmt2Desc);
				delete childFrameDesc; 

				//vraca se kopija pmt2Desc(backup)
				pmt2[page2] = pmt2DescriptorBackup;

				parentFrameDesc->put(pmt2Desc);
				if (originalParentCopyOnWrite == true) {
					parentFrameDesc->setCopyOnWrite();
				}
				return Status::PAGE_FAULT;
			}
		}
		else {
			return Status::OK;
		}
	}
}

PhysicalAddress KernelProcess::getPhysicalAddress(VirtualAddress address) {
	if (PMT::checkVirtualAddressBounds(address) == false) {
		return nullptr;
	}

	PageNum page1, page2, offset;
	Splitter::splitVirtualAddress(address, page1, page2, offset);

	if (pmt1[page1].pmt2Valid == 0) {
		return nullptr;
	}

	PMT2Descriptor* pmt2 = (PMT2Descriptor*)KernelSystem::instance->spaceAllocator->getPhysicalAddressFromFrameNum(pmt1[page1].pmt2FrameNum);
	PMT2Descriptor* pmt2Desc = &pmt2[page2];

	if (pmt2Desc->vmValid == 0) {
		return nullptr;
	}

	PageNum frameAdrAsNum = (PageNum)KernelSystem::instance->frameManager->vmAllocator->getPhysicalAddressFromFrameNum(pmt2Desc->vmFrameNum);
	return (PhysicalAddress)(frameAdrAsNum | offset);
}

void KernelProcess::blockIfThrashing() {
	if (isInThrashing) {
		unique_lock<mutex> lck(thrashingMutex);
		condVarThrashingBlock.wait(lck);
	}
}

Status KernelProcess::createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char * name, AccessType flags){
	//kreira se sharedSegment(ili se proces nakaci na postojeci)

	SharedSegment* sharedSegmentInfo = KernelSystem::instance->sharedSegmentsManager->createSharedSegment(segmentSize, name, myPid);
	
	if (sharedSegmentInfo != nullptr){
	
		sharedSegmentVirtualAddressMap.emplace(name, startAddress);
		virtualAddressSharedSegmentMap.emplace(startAddress, name);

		//kreira se dijeljeni segment za proces,tj potreban broj pmt2Desc i ostalo,
		//prepisu se odgovarajuce informacije o segmentu
		Status result = allocateSharedSegment(startAddress, segmentSize, flags, sharedSegmentInfo);
		if (result != Status::OK) {
			
			disconnectSharedSegment(name);
			
			sharedSegmentVirtualAddressMap.erase(name);
			virtualAddressSharedSegmentMap.erase(startAddress);
			return Status::TRAP;
		}
		else {
			return Status::OK;
		}
	}
	else {
		return Status::TRAP;
	}
}

Status KernelProcess::deleteSharedSegment(const char * name){

	return KernelSystem::instance->sharedSegmentsManager->deleteSharedSegment(name);
}

//odjavljuje proces kao korisnika sharedSegment-a
//i brise segment za njega(pmt2Desc i ostalo...)
Status KernelProcess::disconnectSharedSegment(const char * name) {
	//odjavljuje proces preko sharedSegmentManager-a
	Status status = KernelSystem::instance->sharedSegmentsManager->disconnectSharedSegment(name, myPid);
	
	if (status == Status::OK) {
		//brise dijeljeni segment od procesa
		
		status = deleteSegmentShared(sharedSegmentVirtualAddressMap[name]);
		if (status == Status::OK) {
			
			virtualAddressSharedSegmentMap.erase(sharedSegmentVirtualAddressMap[name]);
			sharedSegmentVirtualAddressMap.erase(name);
		}
	}
	return status;
}

//klonira proces(this) i dodjeljuje mu pid proslijedjen kao argument
Process * KernelProcess::clone(ProcessId pid) {
	//stvaranje novog procesa sa ovim pid-om
	Process* cloneProcess = new Process(pid);
	
	if (cloneProcess->pProcess == nullptr) {
		delete cloneProcess;
		return nullptr;
	}
	//kloniranje svih segmenata roditeljskog procesa
	else if (cloneProcess->pProcess->cloneAllSegments(this) == Status::OK) {
		
		return cloneProcess;
	}
	else {
		delete cloneProcess;
		return nullptr;
	}
}

Status KernelProcess::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags) {
	Status result = allocateNewSegment(startAddress, segmentSize, flags);
	if (result != Status::OK) {
		
		deleteSegmentPrivate(startAddress);
	}

	return result;
}

Status KernelProcess::loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void * content) {

	Status result = allocateNewSegment(startAddress, segmentSize, flags);
	if (result == Status::OK) {
		//uspjesno alociranje,popunjava se sadrzajem
		result = loadSegmentContent(startAddress, segmentSize, content);
		if (result != Status::OK) {
		
			deleteSegmentPrivate(startAddress);
		}
		return result;
	}
	else {
		
		deleteSegmentPrivate(startAddress);
		return result;
	}
}

Status KernelProcess::deleteSegmentPrivate(VirtualAddress startAddress) {
	StdRecursiveMutexLocker locker(kpMutex);

	if (PMT::checkVirtualAddressBounds(startAddress) == false) {
		return Status::TRAP;
	}
	else if (UtilityClass::isVirtualAddressAligned(startAddress) == false) {
		return TRAP;
	}
	else {
		PageNum startPage = Splitter::pageFromVA(startAddress);

		auto it = segments.find(startPage);
		if (it == segments.end()) {
			return Status::TRAP;
		}

		Segment& segmentInfo = it->second;
		if (segmentInfo.getIsShared() == true) {
			// korisnik je pozvao deleteSegment za shared segment, a mora disconnectSharedSegment da pozove za njega
			return Status::TRAP;
		}
		else {
			//implementacija brisanja segmenta
			return deleteSegmentImpl(startAddress);
		}
	}
}

Status KernelProcess::deleteSegmentShared(VirtualAddress startAddress) {
	StdRecursiveMutexLocker locker(kpMutex);

	if (PMT::checkVirtualAddressBounds(startAddress) == false) {
		return Status::TRAP;
	}
	else if (UtilityClass::isVirtualAddressAligned(startAddress) == false) {
		return TRAP;
	}
	else {
		PageNum startPage = Splitter::pageFromVA(startAddress);

		auto it = segments.find(startPage);
		if (it == segments.end())
		{
			return Status::TRAP;
		}

		Segment& segmentInfo = it->second;
		if (segmentInfo.getIsShared() == false) {
			// korisnik je pozvao disconnectSharedSegment za obicni segment, a mora deleteSegment da pozove za njega
			return Status::TRAP;
		}
		else {
			//poziva se fja koja implementira brisanje segmenta
			return deleteSegmentImpl(startAddress);
		}
	}
}

//implementacija brisanja segmenta procesa
Status KernelProcess::deleteSegmentImpl(VirtualAddress startAddress){
	StdRecursiveMutexLocker locker(kpMutex);

	if (PMT::checkVirtualAddressBounds(startAddress) == false) {
		return Status::TRAP;
	}
	else if (UtilityClass::isVirtualAddressAligned(startAddress) == false) {
		return TRAP;
	}
	else {
		PageNum startPage = Splitter::pageFromVA(startAddress);

		auto it = segments.find(startPage);
		if (it == segments.end()) {
			return Status::TRAP;
		}

		Segment segmentInfo = it->second;
		PageNum endPage = startPage + segmentInfo.getNumPages();

		for (PageNum i = startPage; i < endPage; ++i) {
	
			segmentAllocatedPages[i] = true;

			PageNum page1, page2;
			Splitter::splitPage(i, page1, page2);

			PMT2Descriptor* pmt2 = (PMT2Descriptor*)KernelSystem::instance->spaceAllocator->getPhysicalAddressFromFrameNum(pmt1[page1].pmt2FrameNum);
			PMT2Descriptor* pmt2Desc = &pmt2[page2];

			//frameDesc odgovarajuceg okvira u koji se preslikava stranica
			FrameDescriptor* frameDesc = KernelSystem::instance->frameManager->frameDescFromPMT2Desc(pmt2Desc);
			
			//pmt2Desc se uklanja iz korisnika  frame-a
			frameDesc->remove(pmt2Desc);

			//ukoliko frame nema vise korisnika,uklanja se iz OM ili sa diska(ako ima svoj cluster)
			if (frameDesc->getSharedBit() == false) {
				KernelSystem::instance->frameManager->removeProcessFromFrameOwners(frameDesc);
			}
		
			pmt2Desc->vmValid = 0;
			pmt2Desc->vmFrameNum = 0;
			pmt2Desc->diskValid = 0;
			pmt2Desc->diskFrameNum = 0;

			
			pmt1[page1].numValidPMT2Entries--;
			
			if (pmt1[page1].numValidPMT2Entries == 0){
				pmt1[page1].pmt2FrameNum = 0;
				pmt1[page1].pmt2Valid = 0;
			
				KernelSystem::instance->spaceAllocator->freeFrame(pmt2);
			}
		}

		
		segments.erase(it);
		return Status::OK;
	}
}

Status KernelProcess::allocateNewSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags) {
	StdRecursiveMutexLocker locker(kpMutex);

	//pokusaj alociranja praznog segmenta
	if (allocateEmptySegment(startAddress, segmentSize) != Status::OK) {
		return Status::TRAP;
	}

	PageNum startPage = Splitter::pageFromVA(startAddress);

	auto it = segments.find(startPage);
	if (it == segments.end()) {
		return Status::TRAP;
	}

	Segment segmentInfo = it->second;
	PageNum endPage = startPage + segmentInfo.getNumPages();

	//postavljanje informacija o segmentu u hesmapu segmenata procesa
	segments[startPage] = Segment(segmentInfo.getStartPage(), segmentInfo.getNumPages(), flags, segmentInfo.getIsShared());
	it = segments.find(startPage);
	segmentInfo = it->second;


	for (PageNum i = startPage; i < endPage; ++i) {
		PageNum page1, page2;
		Splitter::splitPage(i, page1, page2);

		
		PMT2Descriptor* pmt2 = (PMT2Descriptor*)KernelSystem::instance->spaceAllocator->getPhysicalAddressFromFrameNum(pmt1[page1].pmt2FrameNum);
		PMT2Descriptor* pmt2Desc = &pmt2[page2];


		FrameDescriptor* frameDesc = new FrameDescriptor();


		pmt2Desc->vmValid = 0;
		pmt2Desc->vmFrameNum = 0;
		pmt2Desc->diskValid = 0;
		//pok na frameDesc se inicijalno cuva u polju diskFrameNum
		pmt2Desc->diskFrameNum = (PageNum)frameDesc;
		pmt2Desc->dirty = 0;
		pmt2Desc->referenceBit = 0;
		pmt2Desc->read = segmentInfo.canBeRead();
		pmt2Desc->write = segmentInfo.canBeWritten();
		pmt2Desc->execute = segmentInfo.canBeExecuted();
		pmt2Desc->copyOnWrite = 0;

		frameDesc->put(pmt2Desc);
	}

	return OK;
}

//fja koja klonira segment roditelja
Status KernelProcess::allocateCloneSegment(VirtualAddress startAddress, PageNum segmentSize, KernelProcess* parent){
	StdRecursiveMutexLocker locker(kpMutex);

	
	if (allocateEmptySegment(startAddress, segmentSize) != Status::OK) {
		return Status::TRAP;
	}

	PageNum startPage = Splitter::pageFromVA(startAddress);

	auto it = segments.find(startPage);
	if (it == segments.end()) {
		return Status::TRAP;
	}

	Segment segmentInfo = it->second;

	PageNum endPage = startPage + segmentInfo.getNumPages();

	//segmentInfo od roditelja
	Segment parentSegmentInfo = parent->segments.find(startPage)->second;

	//prepisuju se informacije o segmentu(ukljucujuci i da li je dijeljen)
	//od roditeljskog segmenta koji se klonirao
	segments[startPage] = Segment(segmentInfo.getStartPage(), segmentInfo.getNumPages(), parentSegmentInfo.getAccessType(), segmentInfo.getIsShared());
	it = segments.find(startPage);
	segmentInfo = it->second;

	for (PageNum i = startPage; i < endPage; ++i) {
		PageNum page1, page2;
		Splitter::splitPage(i, page1, page2);


		PMT2Descriptor* pmt2 = (PMT2Descriptor*)KernelSystem::instance->spaceAllocator->getPhysicalAddressFromFrameNum(pmt1[page1].pmt2FrameNum);
		PMT2Descriptor* pmt2Desc = &pmt2[page2];


		PMT2Descriptor* parentPmt2 = (PMT2Descriptor*)KernelSystem::instance->spaceAllocator->getPhysicalAddressFromFrameNum(parent->pmt1[page1].pmt2FrameNum);
		PMT2Descriptor* parentDesc = &parentPmt2[page2];

		//frameDesc okvira u koji se preslikava stranica(preuzeto pomocu pmt2Desc roditelja)
		FrameDescriptor* frameDesc = KernelSystem::instance->frameManager->frameDescFromPMT2Desc(parentDesc);

		//prepisuju se informacije u pmt2Desc klona
		//iz odgovarajuceg frameDesc 
		pmt2Desc->vmValid = frameDesc->getVMValid();
		pmt2Desc->vmFrameNum = parentDesc->vmFrameNum;
		pmt2Desc->diskValid = frameDesc->getDiskValid();
		pmt2Desc->diskFrameNum = parentDesc->diskFrameNum;
		pmt2Desc->dirty = frameDesc->getDirty();
		pmt2Desc->referenceBit = frameDesc->getReferenceBit();
		//prepisivanje prava pristupa stranici
		pmt2Desc->read = segmentInfo.canBeRead();
		pmt2Desc->write = segmentInfo.canBeWritten();
		pmt2Desc->execute = segmentInfo.canBeExecuted();
		//prepisivanje COW od roditelja
		pmt2Desc->copyOnWrite = parentDesc->copyOnWrite;


		//pmt2Desc klona se dodaje kao korisnik odgovarajuceg frameDesc
		frameDesc->put(pmt2Desc);

		//ako okvir nije dijeljen,a ima vise vlasnika,oznacava se kao COW jer ga klon koristi
		if (frameDesc->getSharedBit() == false) {
			if (frameDesc->ownerCount() > 1) {
				frameDesc->setCopyOnWrite();
			}
		}
	}

	return OK;
}

Status KernelProcess::allocateSharedSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, SharedSegment* sharedSegmentInfo){
	StdRecursiveMutexLocker locker(kpMutex);

	PageNum sharedDescNum = sharedSegmentInfo->getNumDescriptors();

	if (segmentSize != sharedDescNum) {
		// mora uvijek da bude ista velicina shared segmenta
		return Status::TRAP;
	}


	if (allocateEmptySegment(startAddress, segmentSize) != Status::OK) {
	
		disconnectSharedSegment(sharedSegmentInfo->getName().c_str());
		return Status::TRAP;
	}


	PageNum startPage = Splitter::pageFromVA(startAddress);

	auto it = segments.find(startPage);
	if (it == segments.end()) {
		return Status::TRAP;
	}


	Segment segmentInfo = it->second;


	PageNum endPage = startPage + segmentInfo.getNumPages();

	segments[startPage] = Segment(segmentInfo.getStartPage(), segmentInfo.getNumPages(), flags, true);
	it = segments.find(startPage);
	segmentInfo = it->second;

	PageNum sharedDescIndex = 0;


	for (PageNum i = startPage; i < endPage; ++i) {

		if (sharedDescIndex >= sharedDescNum) {
			break;
		}

		PageNum page1, page2;
		Splitter::splitPage(i, page1, page2);

		//pmt2Desc odgovarajuce stranice segmenta
		PMT2Descriptor* pmt2 = (PMT2Descriptor*)KernelSystem::instance->spaceAllocator->getPhysicalAddressFromFrameNum(pmt1[page1].pmt2FrameNum);
		PMT2Descriptor* pmt2Desc = &pmt2[page2];

		//frameDesc frame-a za stranicu dijeljenog segmenta
		FrameDescriptor* frameDesc = sharedSegmentInfo->getFrameDescriptor(sharedDescIndex);

		//prepisivanje informacija o valid bitu,br okvira u OM i diskValid
		//iz frameDesc u pmt2Desc
		pmt2Desc->vmValid = frameDesc->getVMValid();
		pmt2Desc->vmFrameNum = frameDesc->getVMFrameNum();
		pmt2Desc->diskValid = frameDesc->getDiskValid();

		//ako je frameDesc validan ili u OM ili na disku
		//tj okvir ove stranice dijeljenog segmenta je bio koristen
		if (frameDesc->isValidAnywhere() == true) {
			pmt2Desc->diskFrameNum = frameDesc->getDiskFrameNum();
		}
		else {
			pmt2Desc->diskFrameNum = (PageNum)frameDesc;
		}

		//prepisivanje odgovarajucih informacija za pmt2Desc iz frameDesc
		//i segmentInfo
		pmt2Desc->dirty = frameDesc->getDirty();
		pmt2Desc->referenceBit = frameDesc->getReferenceBit();
		pmt2Desc->read = segmentInfo.canBeRead();
		pmt2Desc->write = segmentInfo.canBeWritten();
		pmt2Desc->execute = segmentInfo.canBeExecuted();
		pmt2Desc->copyOnWrite = 0;

		//dodaje se pmt2Desc kao korisnik frame-a
		frameDesc->put(pmt2Desc);


		++sharedDescIndex;
	}

	return OK;
}

//fja koja alocira prazan segment pocev od startAddress,velicine segmentSize
Status KernelProcess::allocateEmptySegment(VirtualAddress startAddress, PageNum segmentSize){
	StdRecursiveMutexLocker locker(kpMutex);

	if (!UtilityClass::isVirtualAddressAligned(startAddress)) {
		return Status::TRAP;
	}

	if (startAddress + segmentSize * PMT::PageSize - 1 > PMT::MaxVirtualAddress) {
		return Status::TRAP;
	}

	PageNum startPage = Splitter::pageFromVA(startAddress);
	PageNum endPage = startPage + segmentSize;
	//provjera da slucajno nije vec alocirana neka stranica
	//koja pripada ovom segmentu
	for (PageNum i = startPage; i < endPage; ++i) {
		if (!segmentAllocatedPages[i]) {
			return TRAP;
		}
	}
	PhysicalAddress* framesVec = nullptr;

	//odredjuje se koliko PMT2 ce biti potrebno da se alocira za ovaj segment
	PageNum numPMT2ToAlloc = getNumOfPMT2ToBeAllocated(startPage, endPage);

	if (numPMT2ToAlloc > 0) {
		framesVec = new PhysicalAddress[numPMT2ToAlloc];
	}

	
	if (!KernelSystem::instance->spaceAllocator->allocateFrames(numPMT2ToAlloc, framesVec)) {
		delete[] framesVec;
		return TRAP;
	}

	
	segments[startPage] = Segment(startPage, segmentSize, AccessType::READ, false);

	PageNum nextFreeFrameIndex = 0;
	
	for (PageNum i = startPage; i < endPage; ++i){
		
		segmentAllocatedPages[i] = false;

		PageNum page1, page2;
		Splitter::splitPage(i, page1, page2);

		PMT2Descriptor* pmt2 = nullptr;


		if (pmt1[page1].pmt2Valid) {//alociran je pmt2
			pmt2 = (PMT2Descriptor*)KernelSystem::instance->spaceAllocator->getPhysicalAddressFromFrameNum(pmt1[page1].pmt2FrameNum);
		}
		else {//nije alociran pmt2,uzima se adresa novog okvira iz niza okvira
			//koji su alocirani
			pmt2 = (PMT2Descriptor*)framesVec[nextFreeFrameIndex];
			nextFreeFrameIndex++;
			if (nextFreeFrameIndex > numPMT2ToAlloc) {
				std::exception();
			}
	
			pmt1[page1].pmt2FrameNum = KernelSystem::instance->spaceAllocator->getFrameNumFromPhysicalAddress(pmt2);
			pmt1[page1].pmt2Valid = 1;
			pmt1[page1].numValidPMT2Entries = 0;
		}

		pmt1[page1].numValidPMT2Entries++;
		PMT2Descriptor* pmt2Desc = &pmt2[page2];

		//sva polja se stavljaju na nevalidne vrijednosti
		pmt2Desc->vmValid = 0;
		pmt2Desc->vmFrameNum = 0;
		pmt2Desc->diskValid = 0;
		pmt2Desc->diskFrameNum = UINT32_MAX;
		pmt2Desc->dirty = 0;
		pmt2Desc->referenceBit = 0;
		pmt2Desc->read = 0;
		pmt2Desc->write = 0;
		pmt2Desc->execute = 0;
		pmt2Desc->copyOnWrite = 0;
	}

	delete[] framesVec;
	return OK;
}

//punjenje sadrzaja segmenta
Status KernelProcess::loadSegmentContent(VirtualAddress startAddress, PageNum segmentSize, void * content){
	PageNum startPage = Splitter::pageFromVA(startAddress);
	PageNum endPage = startPage + segmentSize;

	Status status = Status::OK;


	for (PageNum i = startPage; i < endPage; ++i){
		PageNum page1, page2;
		Splitter::splitPage(i, page1, page2);

		PMT2Descriptor* pmt2 = (PMT2Descriptor*)KernelSystem::instance->spaceAllocator->getPhysicalAddressFromFrameNum(pmt1[page1].pmt2FrameNum);
		PMT2Descriptor* pmt2Desc = &pmt2[page2];

		//dohvatanje adrese okvira u koji se preslikava stranica
		PhysicalAddress frameAdr = KernelSystem::instance->frameManager->loadFrameInVM(pmt2Desc);
		
		//setuje se dirty bit,upisuje se odgovarajuci sadrzaj
		if (frameAdr != nullptr) {
			pmt2Desc->dirty = 1;
			memcpy(frameAdr, (unsigned char*)content, PMT::PageSize);
			content = ((unsigned char*)content) + PMT::PageSize;
		}
		else {
			
			deleteSegmentPrivate(startAddress);
			status = Status::PAGE_FAULT;
			break;
		}
	}

	return status;
}

PageNum KernelProcess::getNumOfPMT2ToBeAllocated(PageNum startPage, PageNum endPage) {
	PageNum numPMT2ToAlloc = 0;

	PageNum entriesInPMT1 = 1 << PMT::Page1BitNum;
	PageNum i = startPage;
	PageNum endPageAdjusted;
	if (Splitter::page2FromPage(endPage) == 0) {
		endPageAdjusted = endPage;
	}
	else {

		PageNum page1 = Splitter::page1FromPage(endPage);
		endPageAdjusted = Splitter::pageFromPage1AndPage2(page1 + 1, 0);
	}

	while (i < endPageAdjusted) {
		PageNum page1 = Splitter::page1FromPage(i);

		if (pmt1[page1].pmt2Valid == 0) {
			++numPMT2ToAlloc;
		}

		i += entriesInPMT1;
	}

	return numPMT2ToAlloc;
}

Status KernelProcess::isAccessAllowed(const PMT2Descriptor * pmt2Desc, AccessType type) {
	
	if (pmt2Desc == nullptr) return Status::PAGE_FAULT;
	
	if ((type == AccessType::READ) && (pmt2Desc->read == 0)) return Status::PAGE_FAULT;
	if ((type == AccessType::WRITE) && (pmt2Desc->write == 0)) return Status::PAGE_FAULT;
	if ((type == AccessType::READ_WRITE) && ((pmt2Desc->read == 0) || (pmt2Desc->write == 0))) return Status::PAGE_FAULT;
	if ((type == AccessType::EXECUTE) && (pmt2Desc->execute == 0)) return Status::PAGE_FAULT;
	
	return Status::OK;
}

Status KernelProcess::shouldHandleCOW(const PMT2Descriptor* pmt2Desc) {
	if (pmt2Desc == nullptr) {
		return Status::TRAP;
	}
	else if (pmt2Desc->write == 1) {
		return Status::TRAP;
	}
	else if (pmt2Desc->copyOnWrite == 0) {
		return Status::TRAP;
	}
	else if (lastAccessData.accessType == AccessType::READ_WRITE && pmt2Desc->read == 0) {
		return Status::TRAP;
	}
	else if (lastAccessData.accessType != AccessType::WRITE) {
		return Status::TRAP;
	}
	else {
		return Status::OK;
	}
}

//klonira sve segmente roditeljskog procesa,ciji je pokazivac
//proslijedjen kao argument
//obicne segmente klonira,a zakaci se na dijeljene segmente roditeljskog procesa
Status KernelProcess::cloneAllSegments(KernelProcess * parent) {

	StdRecursiveMutexLocker lockerParent(parent->kpMutex);
	StdRecursiveMutexLocker locker(kpMutex);

	//prolazak kroz sve segmente roditeljskog procesa
	for (auto it = parent->segments.begin(); it != parent->segments.end(); ++it) {
		
		Status status = Status::OK;
		//ako dati segment roditelja nije dijeljeni segment
		if (it->second.getIsShared() == false) {
		
			status = cloneOneSegment(parent, Splitter::virtualAddressFromPage(it->second.getStartPage()), it->second.getNumPages());
		}
		else {//dati segment roditelja je dijeljen,klonirani proces mora da se zakaci na dijeljeni segment
			//kreira se dijeljeni segment,tj klonirani procesa se zakaci na postojeci
			
			
			VirtualAddress address = Splitter::virtualAddressFromPage(it->second.getStartPage());
		
			status = createSharedSegment(address, it->second.getNumPages(), parent->virtualAddressSharedSegmentMap.find(address)->second.c_str(), it->second.getAccessType());
		}

		//doslo do greske,rollback
		if (status != Status::OK) {
			while (segments.empty() == false) {
				VirtualAddress address = Splitter::virtualAddressFromPage(segments.begin()->second.getStartPage());
				//ako segment nije dijeljen,brise se
				if (it->second.getIsShared() == false) {
					deleteSegmentPrivate(address);
				}
				else {//u suprotnom,dijeljen je,klonirani proces se otkaci za dijeljenog segmenta
					disconnectSharedSegment(virtualAddressSharedSegmentMap[address].c_str());
				}
			}
			return status;
		}
	}

	return Status::OK;
}

Status KernelProcess::cloneOneSegment(KernelProcess * parent, VirtualAddress startAddress, PageNum segmentSize) {
	//kloniranje jednog segmenta roditelja
	Status result = allocateCloneSegment(startAddress, segmentSize, parent);
	if (result != Status::OK) {
		//nije uspjelo kloniranje,segment se brise
		deleteSegmentPrivate(startAddress);
	}

	return result;
}
