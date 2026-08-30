
#pragma once


#ifdef __x86_64__
typedef unsigned long      size_t;
#else
typedef unsigned int      size_t;
#endif

#define NULL ((void*)(uintptr_t)0)

