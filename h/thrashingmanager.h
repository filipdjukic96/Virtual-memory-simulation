#pragma once

#include "vm_declarations.h"
#include <vector>

class KernelProcess;


//klasa koja handle-uje thrashing
class ThrashingManager {
public:
	ThrashingManager();
	void processThrashing();
	PageNum counterForThrashingProcessing;
	void splitProcessesForThrashing(std::vector<KernelProcess*>& active, std::vector<KernelProcess*>& thrashing);
	PageNum calcTotalNeededSize(const std::vector<KernelProcess*>& active);
};
