// File: vm_declarations.h
#pragma once

typedef unsigned long PageNum;
typedef unsigned long VirtualAddress;
typedef void* PhysicalAddress;
typedef unsigned long Time;
enum Status { OK, PAGE_FAULT, TRAP };
enum AccessType { READ, WRITE, READ_WRITE, EXECUTE };
typedef unsigned ProcessId;

#define PAGE_SIZE 1024
