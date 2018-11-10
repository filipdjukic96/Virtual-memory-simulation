#include "ClusterAllocator.h"
//#include <iostream>
#include "part.h"
#include "vm_declarations.h"
#include <mutex>
#include "pmt.h"
#include "stdmutexlocker.h"
using namespace std;




ClusterAllocator::ClusterAllocator(Partition* partition_) : partition(partition_) {
	initialize(partition_);
}


ClusterNo ClusterAllocator::allocateFrame() {
	StdRecursiveMutexLocker lockGuard(clusterMutex);

	if (clusterAllocCounter < numOfClusters) {
		ClusterNo returnValue = clusterAllocCounter;
		clusterAllocCounter++;
		return returnValue;
	}

	//UINT32_MAX tj (1<<32) kao nevalidna vrijednost za broj cluster-a
	if (clusterStack.empty()) return UINT32_MAX;

	ClusterNo returnValue = clusterStack.front();
	clusterStack.pop_front();
	return returnValue;
}



void ClusterAllocator::freeFrame(ClusterNo clusterNum) {
	StdRecursiveMutexLocker lockGuard(clusterMutex);

	if (clusterNum >= numOfClusters) return;

	clusterStack.push_front(clusterNum);
}


void ClusterAllocator::initialize(Partition * partition)
{
	numOfClusters = partition->getNumOfClusters();

	//ako je broj clustera veci nego sto mozemo da predstavimo sa 32 bita (1<<32)
	if (numOfClusters >= (UINT32_MAX)) {
		numOfClusters = UINT32_MAX - 1;
	}

	clusterAllocCounter = 0;

}

