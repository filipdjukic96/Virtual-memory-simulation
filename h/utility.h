#pragma once
#include "vm_declarations.h"

class UtilityClass{
public:
	static bool isPhysicalAddressAligned(PhysicalAddress address);
	static PhysicalAddress	alignPhysicalAddress(PhysicalAddress address);
	static bool isVirtualAddressAligned(VirtualAddress address);

private:
	UtilityClass() = delete;
};
