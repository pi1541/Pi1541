#ifndef __circle_types_h__
#define __circle_types_h__

#include <circle/types.h>

#include "circle-kernel.h"
typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef int TUSBKeyboardDevice;
//typedef char                s8;
//typedef short               s16;
//typedef int                 s32;
#if (AARCH == 64)
typedef unsigned long KTHType;
typedef long unsigned int   u64;
#else
typedef unsigned KTHType;
#endif
#endif