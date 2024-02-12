/*

    Part of the Raspberry-Pi Bare Metal Tutorials
    Copyright (c) 2013-2015, Brian Sidebotham
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
        this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
        this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef RPI_BASE_H
#define RPI_BASE_H

#ifdef __ASSEMBLER__

#if defined(RPI2) || defined(RPI3)
    #define PERIPHERAL_BASE     0x3F000000
#else
    #define PERIPHERAL_BASE     0x20000000
#endif

#else

#if !defined (__CIRCLE__)

typedef unsigned KTHType;        /* needed for backward compatibility, Circle 64 bit builds, KernelHandleTimer type, 'unsinged' for 32 bit */

#if defined(RPI2) || defined(RPI3)
    #define PERIPHERAL_BASE     0x3F000000UL
#else
    #define PERIPHERAL_BASE     0x20000000UL
#endif

//#define GPU_IO_BASE				0x7E000000
//#define GPU_CACHED_BASE			0x40000000
//#define GPU_UNCACHED_BASE		0xC0000000
//
//#if defined ( RPI2 ) || defined (RPI3)
//#define GPU_MEM_BASE	GPU_UNCACHED_BASE
//#else
//#define GPU_MEM_BASE	GPU_CACHED_BASE
//#endif

//#define MEM_COHERENT_REGION		0x400000
#else
#include <circle/bcm2835.h>
//#include <circle/bcm2711.h>
#define PERIPHERAL_BASE     ARM_IO_BASE
#endif

#include <stdint.h>

typedef volatile uint32_t rpi_reg_rw_t;
typedef volatile const uint32_t rpi_reg_ro_t;
typedef volatile uint32_t rpi_reg_wo_t;

typedef volatile uint64_t rpi_wreg_rw_t;
typedef volatile const uint64_t rpi_wreg_ro_t;

#endif /* ASSEMBLER */
#endif
