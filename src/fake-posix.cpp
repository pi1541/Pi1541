#include <circle/timer.h>
#include <time.h>
#include <iostream>
#include "fake-posix.h"

extern "C" 
{
int clock_gettime(clockid_t clockid, struct timespec *tp)
{
    unsigned t = CTimer::GetClockTicks();
    tp->tv_sec = t / 1000000L;
    tp->tv_nsec = (t % 1000000L) * 1000;
    return 0;
}
}