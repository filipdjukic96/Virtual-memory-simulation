#pragma once

#include "vm_declarations.h"
#include <vector>
#include <mutex>




struct FreeListElem {
	FreeListElem* next;
};

class PMTAllocator {
public:
	PMTAllocator(PhysicalAddress pmtSpace, PageNum pmtSpaceSize);

	PageNum getFrameNumFromPhysicalAddress(PhysicalAddress frameAdr);
	PhysicalAddress getPhysicalAddressFromFrameNum(PageNum frameNum);

	PhysicalAddress allocateFrame();
	bool allocateFrames(PageNum numFrames, PhysicalAddress* addresses);

	void freeFrame(PhysicalAddress frameAdr);


private:

	void initialize(PhysicalAddress pmtSpace, PageNum pmtSpaceSize);

	FreeListElem* framesHead;

	unsigned char* buffer;
	unsigned char* finalByte;
	PageNum sizeInFrames;
	PageNum freeFrames;

	std::recursive_mutex pmtMutex;
};
