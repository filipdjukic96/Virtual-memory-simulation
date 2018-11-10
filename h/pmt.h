#pragma once

#include "vm_declarations.h"

//usluzna klasa 
class PMT {
public:
	static const PageNum PageSize = PAGE_SIZE;

	static const PageNum PageBitNum = 14;
	static const PageNum PagesNum = 1 << PageBitNum;
	static const PageNum MaxVirtualAddress = 0xFFFFFF;

	static const PageNum Page1BitNum = 7;
	static const PageNum Page2BitNum = 7;
	static const PageNum OffsetBitNum = 10;

	static const PageNum Page1BitMask = (1 << Page1BitNum) - 1;
	static const PageNum Page2BitMask = (1 << Page2BitNum) - 1;
	static const PageNum OffsetBitMask = (1 << OffsetBitNum) - 1;

	static bool checkVirtualAddressBounds(VirtualAddress address);
private:
	PMT() = delete;
};

#pragma pack(push, 1)
struct PMT1Descriptor {
	unsigned pmt2Valid : 1;
	unsigned numValidPMT2Entries : 31;
	unsigned pmt2FrameNum : 32;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PMT2Descriptor {
	unsigned vmValid : 1;

	unsigned diskValid : 1;

	unsigned dirty : 1;

	unsigned referenceBit : 1;

	unsigned read : 1;
	unsigned write : 1;
	unsigned execute : 1;
	unsigned copyOnWrite : 1;
	
	unsigned vmFrameNum : 24;
	unsigned diskFrameNum : 32;
};
#pragma pack(pop)


struct FrameDescriptor {
	FrameDescriptor();
	~FrameDescriptor();

	PageNum ownerCount();
	void put(PMT2Descriptor* pmt2Desc);
	void remove(PMT2Descriptor* pmt2Desc);

	bool getVMValid();
	void setVMValid();
	void resetVMValid();

	PageNum getVMFrameNum();
	void setVMFrameNum(PageNum frameNum);
	void resetVMFrameNum();

	bool getDiskValid();
	void setDiskValid();
	void resetDiskValid();

	PageNum getDiskFrameNum();
	void setDiskFrameNum(PageNum frameDiskNum);
	void resetDiskFrameNum();

	bool isValidAnywhere();

	bool getDirty();
	void setDirty();
	void resetDirty();

	bool getReferenceBit();
	void setReferenceBit();
	void resetReferenceBit();

	bool getSharedBit();
	void setSharedBit();
	void resetSharedBit();

	void setCopyOnWrite();
	void resetCopyOnWrite();

private:
	//element liste pmt2Desc
	struct Elem {
		Elem* next;
		PMT2Descriptor* pmt2Desc;

		Elem(PMT2Descriptor* pmt2Desc) {
			this->pmt2Desc = pmt2Desc;
			next = nullptr;
		}
	};

	
	PageNum listSize;
	//glava liste pmt2Desc
	Elem* pmt2Head;

	bool vmValid;
	bool diskValid;

	PageNum vmFrameNum;
	PageNum diskFrameNum;

	bool sharedBit;
};
