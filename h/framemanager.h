#pragma once

#include "vm_declarations.h"
#include "pmt.h"
#include <vector>
#include <mutex>

class VMAllocator;
class Partition;
class ClusterAllocator;



class FrameManager{
	friend class KernelProcess;
	friend class ThrashingManager;
public:
	FrameManager(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, Partition* partition);

	PhysicalAddress loadFrameInVM(PMT2Descriptor* pmt2Desc);
	void removeProcessFromFrameOwners(FrameDescriptor* frameDesc);
	void removeProcessFromFrameOwners(PMT2Descriptor* pmt2Desc);

	FrameDescriptor* frameDescFromPMT2Desc(const PMT2Descriptor* pmt2Desc);

	PageNum findFrameToFlush();
	PageNum flushFrame();

private:
	std::recursive_mutex framesMutex;
	std::vector<FrameDescriptor*> clusterDescriptors;
	std::vector<FrameDescriptor*> vmDescriptors;
	PageNum clockHand;
	Partition* partition;
	VMAllocator* vmAllocator;
	ClusterAllocator* clusterAllocator;
};
