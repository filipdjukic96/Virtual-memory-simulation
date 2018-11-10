#pragma once

#include "vm_declarations.h"
#include <vector>
#include <mutex>
#include <deque>
#include "part.h"
class Partition;





class ClusterAllocator {
public:
	ClusterAllocator(Partition* partition);
	ClusterNo allocateFrame();
	void freeFrame(ClusterNo cluster);
	ClusterNo size() {
		return numOfClusters;
	}

private:
	void initialize(Partition* partition);

	ClusterNo numOfClusters;
	Partition* partition;
	std::recursive_mutex clusterMutex;
	ClusterNo clusterAllocCounter;
	std::deque<ClusterNo> clusterStack;

};
