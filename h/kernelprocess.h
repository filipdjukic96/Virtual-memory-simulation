#pragma once

#include "vm_declarations.h"
#include "pmt.h"
#include <condition_variable>
#include <mutex>
#include "kernelsystem.h"
#include <unordered_set>
#include <unordered_map>
#include <bitset>
#include "sharedsegment.h"
#include "segment.h"



class Process;

class KernelProcess{
	friend class KernelSystem;
	friend class ThrashingManager;
public:
	KernelProcess(ProcessId pid, Process* myProcess, PMT1Descriptor* pmt1);
	~KernelProcess();
	ProcessId getProcessId() const;
	Status access(VirtualAddress address, AccessType type);
	Status pageFault(VirtualAddress address);
	PhysicalAddress getPhysicalAddress(VirtualAddress address);
	void blockIfThrashing();

	Status createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char* name, AccessType flags);
	Status deleteSharedSegment(const char* name);
	Status disconnectSharedSegment(const char* name);

	Process* clone(ProcessId pid);

	Status createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags);
	Status loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content);
	Status deleteSegmentPrivate(VirtualAddress startAddress);
	Status deleteSegmentShared(VirtualAddress startAddress);
	Status deleteSegmentImpl(VirtualAddress startAddress);

	Status allocateNewSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags);
	Status allocateSharedSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, SharedSegment* sharedSegmentInfo);
	Status allocateCloneSegment(VirtualAddress startAddress, PageNum segmentSize, KernelProcess* parent);
	Status allocateEmptySegment(VirtualAddress startAddress, PageNum segmentSize);

	Status loadSegmentContent(VirtualAddress startAddress, PageNum segmentSize, void* content);

	PageNum getNumOfPMT2ToBeAllocated(PageNum startPage, PageNum endPage);

	Status isAccessAllowed(const PMT2Descriptor* pmt2Desc, AccessType type);
	Status shouldHandleCOW(const PMT2Descriptor* pmt2Desc);

	Status cloneAllSegments(KernelProcess* parent);
	Status cloneOneSegment(KernelProcess* parent, VirtualAddress startAddress, PageNum segmentSize);
private:
	ProcessId myPid;
	Process* myProcess;

	//hesmapa segmenata procesa po pocetnoj stranici
	std::unordered_map<PageNum, Segment> segments;


	std::unordered_map<std::string, VirtualAddress> sharedSegmentVirtualAddressMap;
	std::unordered_map<VirtualAddress, std::string> virtualAddressSharedSegmentMap;

	std::recursive_mutex kpMutex;

	//bit vektor alociranih stranica procesa
	std::bitset<PMT::PagesNum> segmentAllocatedPages;

	PMT1Descriptor* pmt1;


	bool isInThrashing;

	//trenutni radni skup
	std::unordered_set<PageNum> accessHistory;


	std::condition_variable condVarThrashingBlock;
	std::mutex thrashingMutex;

	//struktura koja cuva infornmacije o posljednjem pristupu
	//virtuelnu adresu i tip pristupa koji je trazen
	struct AccessData {
		AccessData() {
			virtualAddress = 0;
			accessType = AccessType::READ;
		}
		AccessData(VirtualAddress virtualAddress, AccessType accessType) {
			this->virtualAddress = virtualAddress;
			this->accessType = accessType;
		}
		VirtualAddress virtualAddress;
		AccessType accessType;
	};

	AccessData lastAccessData;
};
