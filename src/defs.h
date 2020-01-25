#ifndef DEFS_H
#define DEFS_H

#include "debug.h"
#define PI1581SUPPORT 1
// Indicates a Pi with the 40 pin GPIO connector
// so that additional functionality (e.g. test pins) can be enabled
#if defined(RPIZERO) || defined(RPIBPLUS) || defined(RPI2) || defined(RPI3)
#define HAS_40PINS
#endif

// Pi 2/3 Multicore options
#if defined(RPI2) || defined(RPI3)

// Indicate the platform has multiple cores
#define HAS_MULTICORE

#define USE_GPU

#define USE_HW_MAILBOX

// Indicates we want to make active use of multiple cores
#if defined(RPI3)
#define USE_MULTICORE
#endif

// Needs to match kernel_old setting in config.txt
//#define KERNEL_OLD

// Include instruction histogram in multi core 65tube
//#define HISTOGRAM

#else

#define USE_GPU

#define USE_HW_MAILBOX

#endif

#include "rpi-base.h"

#ifdef USE_HW_MAILBOX
#define MBOX0_READ      (PERIPHERAL_BASE + 0x00B880)
#define MBOX0_STATUS    (PERIPHERAL_BASE + 0x00B898)
#define MBOX0_CONFIG    (PERIPHERAL_BASE + 0x00B89C)
#define MBOX0_EMPTY     (0x40000000)
#define MBOX0_DATAIRQEN (0x00000001)
#endif

#ifdef __ASSEMBLER__

#define GPFSEL0 (PERIPHERAL_BASE + 0x200000)  // controls GPIOs 0..9
#define GPFSEL1 (PERIPHERAL_BASE + 0x200004)  // controls GPIOs 10..19
#define GPFSEL2 (PERIPHERAL_BASE + 0x200008)  // controls GPIOs 20..29
#define GPSET0  (PERIPHERAL_BASE + 0x20001C)
#define GPCLR0  (PERIPHERAL_BASE + 0x200028)
#define GPLEV0  (PERIPHERAL_BASE + 0x200034)
#define GPEDS0  (PERIPHERAL_BASE + 0x200040)
#define FIQCTRL (PERIPHERAL_BASE + 0x00B20C)

#endif // __ASSEMBLER__

#ifdef HAS_40PINS
#define TEST_PIN     (21)
#define TEST_MASK    (1 << TEST_PIN)
#define TEST2_PIN    (20)
#define TEST2_MASK   (1 << TEST2_PIN)
#define TEST3_PIN    (16)
#define TEST3_MASK   (1 << TEST3_PIN)
#endif

#endif
