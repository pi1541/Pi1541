#ifndef DEBUG_H
#define DEBUG_H

#ifdef __ASSEMBLER__
#else
#include <stdio.h>
#endif

#ifdef DEBUG
#define DEBUG_LOG(...) printf(__VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif


#endif