#include "utility.h"
#include "pmt.h"
#include "vm_declarations.h"



PhysicalAddress UtilityClass::alignPhysicalAddress(PhysicalAddress address){
	if (((unsigned long)(address) & PMT::OffsetBitMask) == 0)
		return address;

	unsigned long relativeAddress = (unsigned long)(address) + PMT::PageSize;
	return (PhysicalAddress)(relativeAddress & (~PMT::OffsetBitMask));
}

bool UtilityClass::isPhysicalAddressAligned(PhysicalAddress address) {
	bool result = ((unsigned long)(address) & PMT::OffsetBitMask) != 0;
	return !result;

}

bool UtilityClass::isVirtualAddressAligned(VirtualAddress address) {
	bool result = (address & PMT::OffsetBitMask) != 0;
	return !result;
}