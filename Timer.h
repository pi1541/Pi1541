#ifndef Timer_H
#define Timer_H
#include <stddef.h>
#include "interrupt.h"

// Support functions for uspi
// Based on env\include\uspienv\timer.h

#ifdef __cplusplus
extern "C" {
#endif

#define KERNEL_TIMERS		20

#define HZ		100			// ticks per second

#define MSEC2HZ(msec)	((msec) * HZ / 1000)

	typedef void TKernelTimerHandler(unsigned hTimer, void* pParam, void* pContext);

	typedef struct TKernelTimer
	{
		TKernelTimerHandler* m_pHandler;
		unsigned m_nElapsesAt;
		void* m_pParam;
		void* m_pContext;
	}
	TKernelTimer;

	void TimerSystemInitialize();

#define CLOCKHZ	1000000

	unsigned TimerStartKernelTimer(
		unsigned nDelay,		// in HZ units
		TKernelTimerHandler* pHandler,
		void* pParam,
		void* pContext);
	void TimerCancelKernelTimer(unsigned hTimer);

#ifdef __cplusplus
}
#endif


#endif
