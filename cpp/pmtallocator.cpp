#include "PMTAllocator.h"
#include <iostream>
#include "pmt.h"
#include "utility.h"
#include "stdmutexlocker.h"

using namespace std;





PMTAllocator::PMTAllocator(PhysicalAddress pmtSpace, PageNum pmtSpaceSize) {
	initialize(pmtSpace, pmtSpaceSize);
}


PageNum PMTAllocator::getFrameNumFromPhysicalAddress(PhysicalAddress frameAdr) {
	if (UtilityClass::isPhysicalAddressAligned(frameAdr) == false) {
		std::cout << "frameAdr nije poravnata adresa" << std::endl;
		throw std::exception();
		return 0;
	}
	else {
		unsigned long relativeAddress = (unsigned long)(frameAdr)-(unsigned long)(buffer);
		unsigned long frame = relativeAddress >> PMT::OffsetBitNum;
		return frame;
	}
}


PhysicalAddress PMTAllocator::getPhysicalAddressFromFrameNum(PageNum frameNum) {
	if (frameNum >= sizeInFrames) {
		std::cout << "frameNum je van pmt memorije" << std::endl;
		throw std::exception();
		return 0;
	}
	else {
		unsigned long offset = frameNum << PMT::OffsetBitNum;
		unsigned char* frameAddress = buffer + offset;

		return (PhysicalAddress)(frameAddress);
	}
}


PhysicalAddress PMTAllocator::allocateFrame() {
	StdRecursiveMutexLocker lockGuard(pmtMutex);

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


void PMTAllocator::freeFrame(PhysicalAddress frameAdr) {

	if (UtilityClass::isPhysicalAddressAligned(frameAdr) == false) {
		std::cout << "frameAdr nije poravnata adresa" << std::endl;
		throw std::exception();
	}
	else if (frameAdr < buffer || frameAdr >= finalByte) {
		std::cout << "frameAdr je van pmt memorije" << std::endl;
		throw std::exception();
	}
	else {
		StdRecursiveMutexLocker lockGuard(pmtMutex);

		FreeListElem* newElem = (FreeListElem*)frameAdr;
		newElem->next = framesHead;
		framesHead = newElem;
		freeFrames++;
	}
}

bool PMTAllocator::allocateFrames(PageNum numFrames, PhysicalAddress* addresses) {
	StdRecursiveMutexLocker lockGuard(pmtMutex);

	if (numFrames == 0) {
		return true;
	}
	else if (numFrames > freeFrames || framesHead == nullptr || addresses == nullptr) {
		return false;
	}
	else {

		unsigned long i = 0;

		while (i < numFrames) {

			FreeListElem* allocated = framesHead;
			framesHead = framesHead->next;

			memset(allocated, 0x00, PMT::PageSize);


			addresses[i] = (PhysicalAddress)(allocated);
			freeFrames--;
			++i;
		}

		return true;
	}
}


void PMTAllocator::initialize(PhysicalAddress pmtSpace, PageNum pmtSpaceSize) {


	if (UtilityClass::isPhysicalAddressAligned(pmtSpace) == false) {

		if (--pmtSpaceSize == 0) {
			std::cout << "nema nijedan poravnat okvir u zadatoj pmt memoriji" << std::endl;
			throw std::exception();
		}
		else {
			pmtSpace = UtilityClass::alignPhysicalAddress(pmtSpace);
		}
	}

	buffer = (unsigned char*)(pmtSpace);
	framesHead = (FreeListElem*)(buffer);
	sizeInFrames = freeFrames = pmtSpaceSize;
	finalByte = buffer + (pmtSpaceSize << PMT::OffsetBitNum);

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
