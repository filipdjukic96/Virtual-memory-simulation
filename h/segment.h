#pragma once

#include "vm_declarations.h"



//struktura koja cuva podatke o jednom segmentu
class Segment{
public:
	Segment();
	Segment(PageNum startPage, PageNum numPages, AccessType accessType, bool isShared);
	PageNum getStartPage() const;
	PageNum getNumPages() const;

	bool getIsShared() const;
	void setIsShared();

	bool canBeRead() const;
	bool canBeWritten() const;
	bool canBeExecuted() const;

	AccessType getAccessType() const;
	void setAccessType(AccessType accessType);
private:
	PageNum startPage;
	PageNum numPages;
	AccessType accessType;
	bool isShared;
};

