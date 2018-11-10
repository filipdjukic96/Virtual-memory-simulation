
#pragma once

#include "vm_declarations.h"
#include "pmt.h"

class Splitter{
public:
	static PageNum page1FromPage(PageNum page);

	static PageNum page2FromPage(PageNum page);

	static PageNum pageFromPage1AndPage2(PageNum page1, PageNum page2);

	static PageNum page1FromVA(const VirtualAddress address);

	static PageNum page2FromVA(const VirtualAddress address);

	static PageNum offsetFromVA(const VirtualAddress address);

	static PageNum pageFromVA(VirtualAddress address);

	static VirtualAddress virtualAddressFromPage(PageNum page);

	static void splitVirtualAddress(VirtualAddress address, PageNum& page1, PageNum& page2, PageNum& offset);

	static void splitPage(PageNum page, PageNum& page1, PageNum& page2);

private:
	Splitter() = delete;
};
