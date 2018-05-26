#include "Timer.h"
#include "bcm2835int.h"
#include "rpiHardware.h"

// Support functions for uspi
// Based on env\include\uspienv\timer.h

// The number of 1MHz ticks for 10ms
#define TEN_MILLISECS (CLOCKHZ / HZ)

static unsigned Ticks = 0;

static volatile TKernelTimer	 m_KernelTimer[KERNEL_TIMERS];	// TODO: should be linked list

static void TimerPollKernelTimers()
{
	//EnterCritical();

	unsigned hTimer;
	for (hTimer = 0; hTimer < KERNEL_TIMERS; hTimer++)
	{
		volatile TKernelTimer* pTimer = &m_KernelTimer[hTimer];

		TKernelTimerHandler* pHandler = pTimer->m_pHandler;
		if (pHandler != 0)
		{
			if ((int)(pTimer->m_nElapsesAt - Ticks) <= 0)
			{
				pTimer->m_pHandler = 0;

				(*pHandler)(hTimer + 1, pTimer->m_pParam, pTimer->m_pContext);
			}
		}
	}

	//LeaveCritical();
}

static void TimerInterruptHandler(void* pParam)
{
	DataMemBarrier();

	//assert(read32(ARM_SYSTIMER_CS) & (1 << 3));

	u32 nCompare = read32(ARM_SYSTIMER_C3) + TEN_MILLISECS;
	write32(ARM_SYSTIMER_C3, nCompare);
	if (nCompare < read32(ARM_SYSTIMER_CLO))			// time may drift
	{
		nCompare = read32(ARM_SYSTIMER_CLO) + TEN_MILLISECS;
		write32(ARM_SYSTIMER_C3, nCompare);
	}

	write32(ARM_SYSTIMER_CS, 1 << 3);

	DataMemBarrier();

	++Ticks;

	TimerPollKernelTimers();
}

void TimerSystemInitialize()
{
	InterruptSystemConnectIRQ(ARM_IRQ_TIMER3, TimerInterruptHandler, 0);

	DataMemBarrier();

	write32(ARM_SYSTIMER_CLO, -(30 * CLOCKHZ));	// timer wraps soon, to check for problems

	// Interrupt every 10ms
	write32(ARM_SYSTIMER_C3, read32(ARM_SYSTIMER_CLO) + TEN_MILLISECS);
	
	DataMemBarrier();

	unsigned hTimer;
	for (hTimer = 0; hTimer < KERNEL_TIMERS; hTimer++)
	{
		m_KernelTimer[hTimer].m_pHandler = 0;
	}

}

unsigned TimerStartKernelTimer(unsigned nDelay, TKernelTimerHandler* pHandler, void* pParam, void* pContext)
{
	//EnterCritical();

//	DEBUG_LOG("Timer started\r\n");

	unsigned hTimer;
	for (hTimer = 0; hTimer < KERNEL_TIMERS; hTimer++)
	{
		if (m_KernelTimer[hTimer].m_pHandler == 0)
		{
			break;
		}
	}

	if (hTimer >= KERNEL_TIMERS)
	{
		//LeaveCritical();

		DEBUG_LOG("System limit of kernel timers exceeded\r\n");

		return 0;
	}

	//assert(pHandler != 0);
	m_KernelTimer[hTimer].m_pHandler    = pHandler;
	m_KernelTimer[hTimer].m_nElapsesAt  = Ticks+nDelay;
	m_KernelTimer[hTimer].m_pParam      = pParam;
	m_KernelTimer[hTimer].m_pContext    = pContext;

	//LeaveCritical();

	return hTimer+1;
}

void TimerCancelKernelTimer(unsigned hTimer)
{
	//assert(1 <= hTimer && hTimer <= KERNEL_TIMERS);
	m_KernelTimer[hTimer-1].m_pHandler = 0;
}

