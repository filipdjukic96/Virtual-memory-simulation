#include "splitter.h"


PageNum Splitter::page1FromPage(PageNum page) {

	return ((page >> PMT::Page2BitNum) & PMT::Page1BitMask);
}

PageNum Splitter::page2FromPage(PageNum page) {
	return (page & PMT::Page2BitMask);
}

PageNum Splitter::pageFromPage1AndPage2(PageNum page1, PageNum page2) {
	return (page1 << PMT::Page2BitNum) | page2;
}

PageNum Splitter::page1FromVA(const VirtualAddress address) {
	return ((address >> (PMT::Page2BitNum + PMT::OffsetBitNum)) & PMT::Page1BitMask);
}

PageNum Splitter::page2FromVA(const VirtualAddress address) {
	return ((address >> PMT::OffsetBitNum) & PMT::Page2BitMask);
}

PageNum Splitter::offsetFromVA(const VirtualAddress address) {
	return (address & PMT::OffsetBitMask);
}

PageNum Splitter::pageFromVA(VirtualAddress address) {
	return address >> PMT::OffsetBitNum;
}

VirtualAddress Splitter::virtualAddressFromPage(PageNum page) {
	return page << PMT::OffsetBitNum;
}

void Splitter::splitVirtualAddress(VirtualAddress address, PageNum & page1, PageNum & page2, PageNum & offset) {
	page1 = Splitter::page1FromVA(address);
	page2 = Splitter::page2FromVA(address);
	offset = Splitter::offsetFromVA(address);
}

void Splitter::splitPage(PageNum page, PageNum & page1, PageNum & page2) {
	page1 = page1FromPage(page);
	page2 = page2FromPage(page);
}
