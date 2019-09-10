#include "interrupt.h"
#include "rpiHardware.h"

#define ARM_IC_IRQ_PENDING(irq)	(  (irq) < ARM_IRQ2_BASE	\
				 ? ARM_IC_IRQ_PENDING_1		\
				 : ((irq) < ARM_IRQBASIC_BASE	\
				   ? ARM_IC_IRQ_PENDING_2	\
				   : ARM_IC_IRQ_BASIC_PENDING))
#define ARM_IC_IRQS_ENABLE(irq)	(  (irq) < ARM_IRQ2_BASE	\
				 ? ARM_IC_ENABLE_IRQS_1		\
				 : ((irq) < ARM_IRQBASIC_BASE	\
				   ? ARM_IC_ENABLE_IRQS_2	\
				   : ARM_IC_ENABLE_BASIC_IRQS))
#define ARM_IC_IRQS_DISABLE(irq) (  (irq) < ARM_IRQ2_BASE	\
				 ? ARM_IC_DISABLE_IRQS_1	\
				 : ((irq) < ARM_IRQBASIC_BASE	\
				   ? ARM_IC_DISABLE_IRQS_2	\
				   : ARM_IC_DISABLE_BASIC_IRQS))
#define ARM_IRQ_MASK(irq)	(1 << ((irq) & (ARM_IRQS_PER_REG-1)))

static IRQHandler* IRQHandlers[IRQ_LINES] = { 0 };
static void* Params[IRQ_LINES] = { 0 };

void InterruptSystemInitialize()
{
	InstructionSyncBarrier();

	DataMemBarrier();

	write32(ARM_IC_FIQ_CONTROL, 0);

	write32(ARM_IC_DISABLE_IRQS_1, (u32)-1);
	write32(ARM_IC_DISABLE_IRQS_2, (u32)-1);
	write32(ARM_IC_DISABLE_BASIC_IRQS, (u32)-1);

	// Ack pending IRQs
	write32(ARM_IC_IRQ_BASIC_PENDING, read32(ARM_IC_IRQ_BASIC_PENDING));
	write32(ARM_IC_IRQ_PENDING_1, read32(ARM_IC_IRQ_PENDING_1));
	write32(ARM_IC_IRQ_PENDING_2, read32(ARM_IC_IRQ_PENDING_2));

	DataMemBarrier();
#ifdef EXPERIMENTALZERO
	DisableInterrupts();
#else
	EnableInterrupts();
#endif
}

void InterruptSystemConnectIRQ(unsigned IRQIndex, IRQHandler* handler, void* param)
{
#ifndef EXPERIMENTALZERO
	IRQHandlers[IRQIndex] = handler;
	Params[IRQIndex] = param;

	InterruptSystemEnableIRQ(IRQIndex);
#endif
}

void InterruptSystemDisconnectIRQ(unsigned IRQIndex)
{
	InterruptSystemDisableIRQ(IRQIndex);

	IRQHandlers[IRQIndex] = 0;
	Params[IRQIndex] = 0;
}

void InterruptSystemEnableIRQ(unsigned IRQIndex)
{
#ifndef EXPERIMENTALZERO
	DEBUG_LOG("InterruptSystemEnableIRQ %d\r\n", IRQIndex);
	DataMemBarrier();
	write32(ARM_IC_IRQS_ENABLE(IRQIndex), ARM_IRQ_MASK(IRQIndex));
	DataMemBarrier();
#endif
}

void InterruptSystemDisableIRQ(unsigned IRQIndex)
{
	DataMemBarrier();
	write32 (ARM_IC_IRQS_DISABLE(IRQIndex), ARM_IRQ_MASK(IRQIndex));
	DataMemBarrier();
}

void InterruptHandler(void)
{
//	DEBUG_LOG("InterruptHandler\r\n");

	DataMemBarrier();
	
	//(irq) < ARM_IRQ2_BASE ? ARM_IC_IRQ_PENDING_1 : ((irq) < ARM_IRQBASIC_BASE ? ARM_IC_IRQ_PENDING_2 : ARM_IC_IRQ_BASIC_PENDING

	//for (unsigned IRQIndex = 0; IRQIndex < IRQ_LINES; ++IRQIndex)
	//{
	//	u32 nPendReg = ARM_IC_IRQ_PENDING(IRQIndex);
	//	u32 IRQIndexMask = ARM_IRQ_MASK(IRQIndex);
	//	
	//	if (read32(nPendReg) & IRQIndexMask)
	//	{
	//		IRQHandler* pHandler = IRQHandlers[IRQIndex];

	//		if (pHandler != 0)
	//			(*pHandler)(Params[IRQIndex]);
	//		else
	//			InterruptSystemDisableIRQ(IRQIndex);
	//	}
	//}

	unsigned IRQIndex;
	u32 nPendReg;
	u32 pendValue;

	nPendReg = ARM_IC_IRQ_PENDING_1;
	pendValue = read32(nPendReg);
	for (IRQIndex = 0; IRQIndex < ARM_IRQ2_BASE; ++IRQIndex)
	{
		u32 IRQIndexMask = ARM_IRQ_MASK(IRQIndex);

		if (pendValue & IRQIndexMask)
		{
#ifndef EXPERIMENTALZERO
			IRQHandler* pHandler = IRQHandlers[IRQIndex];

			if (pHandler != 0)
				(*pHandler)(Params[IRQIndex]);
			else
#endif
				InterruptSystemDisableIRQ(IRQIndex);
		}
	}

	nPendReg = ARM_IC_IRQ_PENDING_2;
	pendValue = read32(nPendReg);
	for (;IRQIndex < ARM_IRQBASIC_BASE; ++IRQIndex)
	{
		u32 IRQIndexMask = ARM_IRQ_MASK(IRQIndex);

		if (pendValue & IRQIndexMask)
		{
#ifndef EXPERIMENTALZERO
			IRQHandler* pHandler = IRQHandlers[IRQIndex];

			if (pHandler != 0)
				(*pHandler)(Params[IRQIndex]);
			else
#endif
				InterruptSystemDisableIRQ(IRQIndex);
		}
	}

	nPendReg = ARM_IC_IRQ_BASIC_PENDING;
	pendValue = read32(nPendReg);
	for (;IRQIndex < IRQ_LINES; ++IRQIndex)
	{
		u32 IRQIndexMask = ARM_IRQ_MASK(IRQIndex);

		if (pendValue & IRQIndexMask)
		{
#ifndef EXPERIMENTALZERO
			IRQHandler* pHandler = IRQHandlers[IRQIndex];

			if (pHandler != 0)
				(*pHandler)(Params[IRQIndex]);
			else
#endif
				InterruptSystemDisableIRQ(IRQIndex);
		}
	}

	DataMemBarrier();
}

