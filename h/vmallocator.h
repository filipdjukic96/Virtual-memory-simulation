#pragma once

#include "vm_declarations.h"
#include <mutex>
#include <vector>




class VMAllocator {
public:
	VMAllocator(PhysicalAddress processVMSpace, PageNum processVMSpaceSize);

	PageNum getFrameNumFromPhysicalAddress(PhysicalAddress frameAdr);
	PhysicalAddress getPhysicalAddressFromFrameNum(PageNum frameNum);

	PhysicalAddress allocateFrame();

	void freeFrame(PhysicalAddress frameAdr);

	PageNum getSizeInFrames() const { return sizeInFrames; }

private:
	struct FreeListElem {
		FreeListElem* next;
	};

	FreeListElem* framesHead;

	void initialize(PhysicalAddress processVMSpace, PageNum processVMSpaceSize);

	unsigned char* buffer;
	unsigned char* finalByte;
	PageNum sizeInFrames;
	PageNum freeFrames;

	std::recursive_mutex vmMutex;
};
