#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "posix_time.h"

int count1, count2;

#define TICKS 100

void handler1(int signo, siginfo_t* si, void* ctx)
{
    assert(si->si_value.sival_int == 1);
    count1 += 1;
}

void handler2(int signo, siginfo_t* si, void* ctx)
{
    assert(si->si_value.sival_int == 2);
    count2 += 1;
}

int main()
{
    timer_t t1, t2;
    struct sigevent e1, e2;
    static struct sigaction a1, a2;
    struct itimerspec i1, i2;

    e1.sigev_notify = SIGEV_SIGNAL;
    e1.sigev_signo = SIGALRM;
    e1.sigev_value.sival_int = 1;

    e2.sigev_notify = SIGEV_SIGNAL;
    e2.sigev_signo = SIGUSR1;
    e2.sigev_value.sival_int = 2;
    
    timer_create(CLOCK_REALTIME, &e1, &t1);
    timer_create(CLOCK_REALTIME, &e2, &t2);

    a1.sa_handler = (void*)handler1;
    a2.sa_handler = (void*)handler2;

    sigaction(SIGALRM, &a1, NULL);
    sigaction(SIGUSR1, &a2, NULL);

    i1.it_value.tv_sec = 5;
    i1.it_value.tv_nsec = 0;
    i1.it_interval.tv_sec = 0;
    i1.it_interval.tv_nsec = 500000000;

    i2.it_value.tv_sec = 2;
    i2.it_value.tv_nsec = 0;
    i2.it_interval.tv_sec = 1;
    i2.it_interval.tv_nsec = 0;

    timer_settime(t1, 0, &i1, NULL); 
    timer_settime(t2, 0, &i2, NULL); 
    
    while (count1 != TICKS) { 
	pause();
#if 0
	if (count2 == TICKS/2) { 
	    sleep(10);
	}
#endif
    }
    printf("count1=%d, count2=%d\n", count1, count2);
    
    clock_gettime(CLOCK_REALTIME, &i1.it_value);
    i1.it_value.tv_sec += 1;
    i1.it_interval.tv_sec = 0;
    i1.it_interval.tv_nsec = 0;

    i2.it_value = i1.it_value;
    i2.it_value.tv_sec += 1;
    i2.it_interval.tv_sec = 0;
    i2.it_interval.tv_nsec = 0;

    timer_settime(t1, TIMER_ABSTIME, &i1, NULL); 
    timer_settime(t2, TIMER_ABSTIME, &i2, NULL); 
    
    clock_gettime(CLOCK_REALTIME, &i1.it_value);
    printf("Current time is %ld sec, %ld nsec\n", 
	   i1.it_value.tv_sec, i1.it_value.tv_nsec);
    pause();
    clock_gettime(CLOCK_REALTIME, &i1.it_value);
    printf("Current time is %ld sec, %ld nsec\n", 
	   i1.it_value.tv_sec, i1.it_value.tv_nsec);
    timer_gettime(t2, &i2);
    printf("count1=%d, count2=%d, remaining time %ld seconds and %ld nsec\n",
	   count1, count2, i2.it_value.tv_sec, i2.it_value.tv_nsec);
    pause();
    printf("count1=%d, count2=%d\n", count1, count2);
    clock_gettime(CLOCK_REALTIME, &i1.it_value);
    printf("Current time is %ld sec, %ld nsec\n", 
	   i1.it_value.tv_sec, i1.it_value.tv_nsec);
    count2 = 0;
    i2.it_value.tv_sec = 0;
    i2.it_value.tv_nsec = 15000000;
    i2.it_interval.tv_sec = 0;
    i2.it_interval.tv_nsec = 15000000;
    timer_settime(t2, TIMER_RELTIME, &i2, NULL); 
    while (count2 != 1000) { /* wait 10 min */
	pause();
    }
    clock_gettime(CLOCK_REALTIME, &i2.it_value);
    printf("Current time is %ld sec, %ld nsec\n", 
	   i2.it_value.tv_sec, i2.it_value.tv_nsec);
    printf("Error is %ld usec\n", 
	   i2.it_value.tv_sec*1000000+i2.it_value.tv_nsec/1000-
	   i1.it_value.tv_sec*1000000-i1.it_value.tv_nsec/1000-15000000);
    timer_delete(t1);
    timer_delete(t2);
    return 0;
}


