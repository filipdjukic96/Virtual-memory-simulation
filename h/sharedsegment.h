#pragma once

#include <string>
#include "vm_declarations.h"
#include <vector>
#include <unordered_set>
#include <mutex>
#include "pmt.h"
#include <unordered_map>



//struktura koja cuva podatke o sharedSegmentu
class SharedSegment {
public:
	SharedSegment(const std::string& name, PageNum size);
	bool hasProcess(ProcessId pid) const;
	bool hasAnyProcess() const;
	void addProcess(ProcessId pid);
	ProcessId getFirst() const;
	void removeProcess(ProcessId pid);

	FrameDescriptor* getFrameDescriptor(PageNum i) const;
	void setFrameDescriptor(PageNum i, FrameDescriptor* frameDesc);
	PageNum getNumDescriptors() const;

	PageNum getSize() const;
	const std::string& getName() const;
private:
	//vektor frameDescriptora za dijeljeni segment
	std::vector<FrameDescriptor*> segmentFrameDescriptors;
	//skup vlasnika dijeljenog segmenta
	std::unordered_set<ProcessId> segmentOwners;
	//velicina i ime
	PageNum size;
	std::string name;
};


//manager shared segmenata
class SharedSegmentsManager {
public:
	~SharedSegmentsManager();
	Status disconnectSharedSegment(const std::string& name, ProcessId processId);
	SharedSegment* disconnectAllFromSharedSegment(const std::string& name);
	SharedSegment* createSharedSegment(PageNum segmentSize, const std::string& name, ProcessId processId);
	Status deleteSharedSegment(const std::string& name);
private:
	//hesmapa diljenih segmenata po imenu
	std::unordered_map<std::string, SharedSegment*> segmentsMap;
	std::recursive_mutex myMutex;
};
