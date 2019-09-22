#ifndef interrupt_h
#define interrupt_h

#ifdef __cplusplus
extern "C" {
#endif

#include "rpi-interrupts.h"
#include "bcm2835int.h"

#define	EnableInterrupts()	__asm volatile ("cpsie i")
#define	DisableInterrupts()	__asm volatile ("cpsid ifa, #0x13")

typedef void IRQHandler(void* param);

void InterruptSystemInitialize();

void InterruptSystemConnectIRQ(unsigned IRQIndex, IRQHandler* handler, void* param);
void InterruptSystemDisconnectIRQ(unsigned IRQIndex);

void InterruptSystemEnableIRQ(unsigned IRQIndex);
void InterruptSystemDisableIRQ(unsigned IRQIndex);

void InterruptHandler(void);

#ifdef __cplusplus
}
#endif

#endif
