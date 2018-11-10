#include "VMAllocator.h"
#include "utility.h"
#include <iostream>
#include "stdmutexlocker.h"
#include "pmt.h"
using namespace std;





VMAllocator::VMAllocator(PhysicalAddress processVMSpace, PageNum processVMSpaceSize) {
	initialize(processVMSpace, processVMSpaceSize);
}


PhysicalAddress VMAllocator::getPhysicalAddressFromFrameNum(PageNum frameNum) {
	if (frameNum >= sizeInFrames) {
		std::cout << "PMTAllocator::GetAddressFromFrameNum - frameNum ne pripada pmtSpace memoriji!" << std::endl;
		throw std::exception();
		return nullptr; // da ne iskace warning
	}
	else {
		unsigned long offset = frameNum << PMT::OffsetBitNum;
		unsigned char* frameAddress = buffer + offset;

		return (PhysicalAddress)(frameAddress);
	}
}


PageNum VMAllocator::getFrameNumFromPhysicalAddress(PhysicalAddress frameAdr)
{
	if (UtilityClass::isPhysicalAddressAligned(frameAdr) == false) {
		std::cout << "PMTAllocator::GetFrameNumFromAddress - frameAdr nije poravnata na okvir!" << std::endl;
		throw std::exception();
		return 0;
	}
	else {
		unsigned long relativeAddress = (unsigned long)(frameAdr)-(unsigned long)(buffer);
		unsigned long frame = relativeAddress >> PMT::OffsetBitNum;
		return frame;
	}
}


PhysicalAddress VMAllocator::allocateFrame() {

	StdRecursiveMutexLocker lockGuard(vmMutex);

	if (framesHead == nullptr) return nullptr;
	else if (freeFrames == 0) return nullptr;
	else {

		FreeListElem* allocated = framesHead;
		framesHead = framesHead->next;

		memset(allocated, 0x00, PMT::PageSize);
		freeFrames--;

		return (PhysicalAddress)allocated;
	}
}


void VMAllocator::freeFrame(PhysicalAddress frameAdr) {

	if (UtilityClass::isPhysicalAddressAligned(frameAdr) == false) {
		std::cout << "frameAdr nije poravnata adresa" << std::endl;
		throw std::exception();
	}
	else if (frameAdr < buffer || frameAdr >= finalByte) {
		std::cout << "frameAdr ne pripada vm memoriji" << std::endl;
		throw std::exception();
	}
	else {
		StdRecursiveMutexLocker lockGuard(vmMutex);

		FreeListElem* newElem = (FreeListElem*)frameAdr;
		newElem->next = framesHead;
		framesHead = newElem;
		freeFrames++;
	}
}


void VMAllocator::initialize(PhysicalAddress processVMSpace, PageNum processVMSpaceSize) {

	// max process VM space and swap space
	if (processVMSpaceSize > PMT::PagesNum) {

		processVMSpaceSize = PMT::PagesNum;
	}

	if (UtilityClass::isPhysicalAddressAligned(processVMSpace) == false) {

		if (--processVMSpaceSize == 0) {
			std::cout << "pmtSpaceSize ima 0 poravnatih okvira" << std::endl;
			throw std::exception();
		}
		else {
			processVMSpace = UtilityClass::alignPhysicalAddress(processVMSpace);
		}
	}


	buffer = (unsigned char*)(processVMSpace);
	framesHead = (FreeListElem*)(buffer);
	finalByte = buffer + (processVMSpaceSize << PMT::OffsetBitNum);
	sizeInFrames = freeFrames = processVMSpaceSize;


	FreeListElem* cur = framesHead;
	unsigned char* next = buffer + PMT::PageSize;

	unsigned long i = 0;

	while (i < sizeInFrames - 1) {
		cur->next = (FreeListElem*)(next);
		cur = cur->next;
		next += PMT::PageSize;
		++i;
	}
	cur->next = nullptr;



}
