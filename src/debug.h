#ifndef DEBUG_H
#define DEBUG_H

#ifdef __ASSEMBLER__
#else
#include <stdio.h>
#endif

#if !defined (__CIRCLE__)
#ifdef DEBUG
#define DEBUG_LOG(...) printf(__VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif
#else
#define DEBUG_LOG(...) Kernel.log(__VA_ARGS__)
#endif

#endif