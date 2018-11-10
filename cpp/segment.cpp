#include "segment.h"



Segment::Segment() 
{
	startPage = 0;
	numPages = 0;
	accessType = AccessType::READ;
	isShared = false;
}

Segment::Segment(PageNum startPage, PageNum numPages, AccessType accessType, bool isShared) 
{
	this->startPage = startPage;
	this->numPages = numPages;
	this->accessType = accessType;
	this->isShared = isShared;
}

PageNum Segment::getStartPage() const 
{
	return startPage;
}

PageNum Segment::getNumPages() const
{
	return numPages;
}

bool Segment::getIsShared() const
{
	return isShared;
}

void Segment::setIsShared() 
{
	isShared = true;
}

bool Segment::canBeRead() const
{
	return (accessType == AccessType::READ) || (accessType == AccessType::READ_WRITE);
}

bool Segment::canBeWritten() const
{
	return (accessType == AccessType::WRITE) || (accessType == AccessType::READ_WRITE);
}

bool Segment::canBeExecuted() const
{
	return accessType == AccessType::EXECUTE;
}

AccessType Segment::getAccessType() const 
{
	return accessType;
}

void Segment::setAccessType(AccessType accessType) 
{
	this->accessType = accessType;
}
