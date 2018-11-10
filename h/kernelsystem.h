#pragma once

#include "vm_declarations.h"
#include <random>
#include <vector>
#include "framemanager.h"
#include "pmt.h"
#include <unordered_set>
#include <unordered_map>
#include <mutex>


class PMTAllocator;
class Partition;
class SharedSegment;
class KernelProcess;
class Process;
class SharedSegmentsManager;
class ThrashingManager;



class KernelSystem{

	friend class KernelProcess;
	friend class Process;
	friend class SharedSegmentsManager;
	friend class ThrashingManager;
public:
	KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
		PhysicalAddress pmtSpace, PageNum pmtSpaceSize,
		Partition* partition);
	~KernelSystem();

	KernelProcess* createKernelProcess(ProcessId pid, Process* process);
	void destroyKernelProcess(KernelProcess* kProcess);
	Process* createProcess();

	Process* cloneProcess(ProcessId pid);
	Status access(ProcessId pid, VirtualAddress address, AccessType type);

	Time periodicJob();

private:
	static KernelSystem* instance;
	
	std::recursive_mutex ksMutex;

	//manager dijeljenih segmenata sistema
	SharedSegmentsManager* sharedSegmentsManager;
	//manager thrashing-a
	ThrashingManager* thrashingManager;

	ProcessId nextId;
	PMTAllocator* spaceAllocator;
	FrameManager* frameManager;
	//hashmapa kernel procesa pod pid-u
	std::unordered_map<ProcessId, KernelProcess*> processMap;
};
