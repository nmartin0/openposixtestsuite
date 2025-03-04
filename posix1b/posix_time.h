/*-< POSIX_TIME.H >-------------------------------------------------*--------*/
/* POSIX.1b                   Version 1.0        (c) 1998  GARRET   *     ?  */
/* (POSIX.1b implementation for Linux)                              *   /\|  */
/*                                                                  *  /  \  */
/*                          Created:     25-Aug-98    K.A. Knizhnik * / [] \ */
/*                          Last update: 27-Aug-98    K.A. Knizhnik * GARRET */
/*------------------------------------------------------------------*--------*/
/* Interface of realtime timer and clock                            *        */
/*------------------------------------------------------------------*--------*/

#ifndef __POSIX_TIME__
#define __POSIX_TIME__

#include <sys/time.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" { 
#endif


#define TIMER_MAX 1024

#ifndef __clockid_t_defined
typedef int   clockid_t;
#endif
#ifndef __timer_t_defined 
typedef void* timer_t;
#endif


#define CLOCK_REALTIME (1)

#define TIMER_RELTIME (0)
#define TIMER_ABSTIME (1)

#ifndef SIGEV_SIGNAL

#define SIGEV_SIGNAL (0)
#define SIGEV_NONE   (1)

union sigval { 
    int   sival_int;
    void* sival_ptr;
};

struct sigevent { 
    int          sigev_notify;
    int          sigev_signo;
    union sigval sigev_value;
};

#endif

#ifndef SA_SIGINFO   

#define SA_SIGINFO (0)

typedef struct siginfo_t { 
    union sigval si_value;
} siginfo_t;

#define si_ptr si_value.sival_ptr

#endif

#ifndef __timespec_defined
#define __timespec_defined 1
struct timespec { 
    time_t tv_sec;
    long   tv_nsec;
};
#endif

struct itimerspec { 
    struct timespec it_interval;
    struct timespec it_value;
};

typedef void (*sa_handler_t)(int signo, siginfo_t* si, void* ctx);

int timer_create(clockid_t clock, struct sigevent* spec, timer_t* timer_hdr);

int timer_settime(timer_t timer_hdr, int flag, struct itimerspec* val,
		   struct itimerspec* old);

int timer_gettime(timer_t timer_hdr, struct itimerspec* old);

int timer_getoverrun(timer_t timer_hdr);

int timer_delete(timer_t timer_hdr);

int clock_gettime(clockid_t clock, struct timespec* val);

int clock_settime(clockid_t clock, struct timespec* val);

int clock_getres(clockid_t clock, struct timespec* resolution);

/* Non POSIX functions */

int  compare_time(struct timeval* t1, struct timeval* t2); 

void add_time(struct timeval* t1, struct timeval* t2); 

void subtract_time(struct timeval* t1, struct timeval* t2); 


/* difference in microseconds between real system time 
 * and time measured by timer, after which correction procedure is applied 
 *  to the timer.
 */
extern int timer_max_error; 

#ifdef __cplusplus
} 
#endif

#endif
