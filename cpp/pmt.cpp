#include "pmt.h"
#include <cstdint>


bool PMT::checkVirtualAddressBounds(VirtualAddress address) {
	if (address > PMT::MaxVirtualAddress) {
		return false;
	}
	else {
		return true;
	}
}


FrameDescriptor::FrameDescriptor() {
	listSize = 0;
	pmt2Head = nullptr;
	vmValid = false;
	diskValid = false;
	vmFrameNum = 0;
	diskFrameNum = UINT32_MAX;
	sharedBit = false;
}

FrameDescriptor::~FrameDescriptor() {
	Elem* old = pmt2Head;
	while (old != nullptr) {
		pmt2Head = pmt2Head->next;
		delete old;
		old = pmt2Head;
	}
}


void FrameDescriptor::put(PMT2Descriptor* pmt2Desc) {
	Elem* newDescriptorElem = new Elem(pmt2Desc);
	newDescriptorElem->next = pmt2Head;
	pmt2Head = newDescriptorElem;
	++listSize;
}

void FrameDescriptor::remove(PMT2Descriptor* pmt2Desc) {
	for (Elem *prev = nullptr, *cur = pmt2Head; cur != nullptr; prev = cur, cur = cur->next) {
		if (cur->pmt2Desc == pmt2Desc) {
			if (prev != nullptr) {
				prev->next = cur->next;
			}
			else {
				pmt2Head = cur->next;
			}

			delete cur;
			--listSize;
			return;
		}
	}
}

PageNum FrameDescriptor::ownerCount() {
	return listSize;
}

//da li je iko upisivao u frame
bool FrameDescriptor::getDirty() {
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		if (cur->pmt2Desc->dirty == 1) {
			return true;
		}
	}
	return false;
}


void FrameDescriptor::setDirty() {
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->dirty = 1;
	}
}


void FrameDescriptor::resetDirty() {
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->dirty = 0;
	}
}

bool FrameDescriptor::getReferenceBit() {
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		if (cur->pmt2Desc->referenceBit == 1) {
			return true;
		}
	}
	return false;
}

void FrameDescriptor::setReferenceBit() {
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->referenceBit = 1;
	}
}

void FrameDescriptor::resetReferenceBit() {
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->referenceBit = 0;
	}
}

bool FrameDescriptor::getVMValid() {
	return vmValid;
}

void FrameDescriptor::setVMValid() {
	vmValid = 1;
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->vmValid = 1;
	}
}

void FrameDescriptor::resetVMValid() {
	vmValid = 0;
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->vmValid = 0;
	}
}

PageNum FrameDescriptor::getVMFrameNum() {
	return vmFrameNum;
}

void FrameDescriptor::setVMFrameNum(PageNum frameNum) {
	vmFrameNum = frameNum;
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->vmFrameNum = frameNum;
	}
}

void FrameDescriptor::resetVMFrameNum() {
	vmFrameNum = 0;
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->vmFrameNum = 0;
	}
}

bool FrameDescriptor::getDiskValid() {
	return diskValid;
}

void FrameDescriptor::setDiskValid() {
	diskValid = 1;
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->diskValid = 1;
	}
}

void FrameDescriptor::resetDiskValid() {
	diskValid = 0;
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->diskValid = 0;
	}
}

PageNum FrameDescriptor::getDiskFrameNum() {
	return diskFrameNum;
}

void FrameDescriptor::setDiskFrameNum(PageNum frameDiskNum) {
	diskFrameNum = frameDiskNum;
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->diskFrameNum = frameDiskNum;
	}
}

void FrameDescriptor::resetDiskFrameNum() {
	diskFrameNum = UINT32_MAX;
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		cur->pmt2Desc->diskFrameNum = UINT32_MAX;
	}
}

//da li je frame u OM ili na disku
bool FrameDescriptor::isValidAnywhere() {
	return getVMValid() == true || getDiskValid() == true;
}

//svim pmt2Desc setuje COW(ako im je write==1)
void FrameDescriptor::setCopyOnWrite() {
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		if (cur->pmt2Desc->write == 1) {
			cur->pmt2Desc->write = 0;
			cur->pmt2Desc->copyOnWrite = 1;
		}
	}
}

//svim pmt2Desc resetuje COW(i vraca write=1)
void FrameDescriptor::resetCopyOnWrite() {
	for (Elem* cur = pmt2Head; cur != nullptr; cur = cur->next) {
		if (cur->pmt2Desc->copyOnWrite == 1) {
			cur->pmt2Desc->copyOnWrite = 0;
			cur->pmt2Desc->write = 1;
		}
	}
}

bool FrameDescriptor::getSharedBit() {
	return sharedBit;
}

void FrameDescriptor::setSharedBit() {
	sharedBit = 1;
}

void FrameDescriptor::resetSharedBit() {
	sharedBit = 0;
}
