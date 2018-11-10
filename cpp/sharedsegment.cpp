#include "sharedsegment.h"
#include "kernelsystem.h"
#include "stdmutexlocker.h"

using namespace std;


SharedSegmentsManager::~SharedSegmentsManager() {
	StdRecursiveMutexLocker locker(myMutex);
	//brisanje podataka o dijeljenim segmentima iz hesmape
	for (auto it = segmentsMap.begin(); it != segmentsMap.end(); ++it) {
		delete it->second;
	}
	segmentsMap.clear();
}

//odjavljuje process sa pid-om processId kao korisnika segmenta
Status SharedSegmentsManager::disconnectSharedSegment(const std::string & name, ProcessId processId) {
	StdRecursiveMutexLocker locker(myMutex);
	auto it = segmentsMap.find(name);
	if (it == segmentsMap.end()) {
		return Status::TRAP;
	}
	it->second->removeProcess(processId);
	return Status::OK;
}

//fja koja sve procese odjavjuje kao korisnike sharedSegmenta
SharedSegment* SharedSegmentsManager::disconnectAllFromSharedSegment(const string& name) {
	StdRecursiveMutexLocker locker(myMutex);

	SharedSegment* sharedSegmentInfo = nullptr;

	auto it = segmentsMap.find(name);
	if (it != segmentsMap.end()) {

		sharedSegmentInfo = it->second;

		while (true) {
			
			if (sharedSegmentInfo->hasAnyProcess() == 0) {
				break;
			}
			
			if (disconnectSharedSegment(name, sharedSegmentInfo->getFirst()) == false) {
				sharedSegmentInfo = nullptr;
				break;
			}
		}
	}
	else {
		sharedSegmentInfo = nullptr;
	}

	return sharedSegmentInfo;
}

//fja za kreiranje dijeljenog segmenta za proces sa processId pid-om
//odnosno za kreiranje informacija o sharedSegmentu(ili dodavanje procesa kao korisnika)
//ukoliko sharedSegment vec postoji
SharedSegment* SharedSegmentsManager::createSharedSegment(PageNum segmentSize, const std::string & name, ProcessId processId) {
	
	StdRecursiveMutexLocker locker(myMutex);
	auto it = segmentsMap.find(name);
	if (it != segmentsMap.end()) {//dati dijeljeni segment postoji u hesmapi shared segmenata
		SharedSegment* sharedSegmentInfo = it->second;

		if (sharedSegmentInfo->getSize() == segmentSize) {
			//dodaje se proces kao korisnik dijeljenog segmenta
			sharedSegmentInfo->addProcess(processId);
			return sharedSegmentInfo;
		}
		else {
			return nullptr;
		}
	}
	else {//ne postoji dijeljeni segment datog imena,kreira se novi
		SharedSegment* sharedSegmentInfo = new SharedSegment(name, segmentSize);

		for (size_t i = 0; i < segmentSize; ++i) {
			//alociraju se frameDesc za svaku stranicu dijeljenog segmenta
			FrameDescriptor* frameDesc = new FrameDescriptor();

			frameDesc->setSharedBit();

			sharedSegmentInfo->setFrameDescriptor(i, frameDesc);
		}

		sharedSegmentInfo->addProcess(processId);

		segmentsMap[name] = sharedSegmentInfo;
		return sharedSegmentInfo;
	}
}

Status SharedSegmentsManager::deleteSharedSegment(const std::string & name) {
	StdRecursiveMutexLocker locker(myMutex);

	//odjavljuje sve procese kao korisnike sharedSegmenta
	SharedSegment* sharedSegmentInfo = disconnectAllFromSharedSegment(name);

	if (sharedSegmentInfo == nullptr) {
		return TRAP;
	}

	//za sve frameDesc dijeljenog segmenta,brisu se,oslobadjaju iz OM i/ili sa particije

	for (size_t i = 0; i < sharedSegmentInfo->getNumDescriptors(); ++i) {
		FrameDescriptor* frameDesc = sharedSegmentInfo->getFrameDescriptor(i);
		KernelSystem::instance->frameManager->removeProcessFromFrameOwners(frameDesc);
		delete frameDesc;
	}

	auto it = segmentsMap.find(name);
	if (it != segmentsMap.end()) {
		segmentsMap.erase(it);
	}

	delete sharedSegmentInfo;

	return Status::OK;
}

SharedSegment::SharedSegment(const std::string & name, PageNum size) {
	this->name = name;
	this->size = size;
	//vektor frameDescriptor-a se postavlja na velicinu dijeljenog segmenta
	segmentFrameDescriptors.resize(size, nullptr);
}

bool SharedSegment::hasProcess(ProcessId pid) const {
	return segmentOwners.find(pid) == segmentOwners.end();
}

bool SharedSegment::hasAnyProcess() const {
	return segmentOwners.empty() == false;
}

void SharedSegment::addProcess(ProcessId pid) {
	segmentOwners.emplace(pid);
}

ProcessId SharedSegment::getFirst() const {
	return *segmentOwners.begin();
}

void SharedSegment::removeProcess(ProcessId pid) {
	segmentOwners.erase(pid);
}

FrameDescriptor * SharedSegment::getFrameDescriptor(PageNum i) const {
	return segmentFrameDescriptors[i];
}

void SharedSegment::setFrameDescriptor(PageNum i, FrameDescriptor * frameDesc) {
	segmentFrameDescriptors[i] = frameDesc;
}

PageNum SharedSegment::getNumDescriptors() const {
	return segmentFrameDescriptors.size();
}

PageNum SharedSegment::getSize() const {
	return size;
}

const std::string & SharedSegment::getName() const {
	return name;
}
