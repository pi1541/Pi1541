#ifndef interrupt_h
#define interrupt_h

#ifdef __cplusplus
extern "C" {
#endif

#include "rpi-interrupts.h"

#define	EnableInterrupts()	__asm volatile ("cpsie i")
#define	DisableInterrupts()	__asm volatile ("cpsid i")

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
