//#include <mutex.h>
//#define pthread_mutex_t mutex
#define sem_t struct semaphore

#define sem_wait down
#define sem_post up

typedef struct sched_param { int sched_priority; } sched_param;
#define pthread_getschedparam(...) 

#define sched_yield(...)
#define PTHREAD_STACK_MIN 4096

#define pthread_attr_setstack(a, b, c) pthread_attr_setstacksize(a, c)
#define pthread_attr_setschedpolicy(...) 
#define pthread_detach(...)
#define _POSIX_TIMERS 1
#define sem_init(a, b, c) sema_init(a, c)
#define sem_destroy(a)
#include <delay.h>
#define usleep udelay
#define sleep(a) msleep ((a) * 1000)