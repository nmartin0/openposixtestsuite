/*-< POSIX_TIME.C >-------------------------------------------------*--------*/
/* POSIX.1b                   Version 1.0        (c) 1998  GARRET   *     ?  */
/* (POSIX.1b implementation for Linux)                              *   /\|  */
/*                                                                  *  /  \  */
/*                          Created:     25-Aug-98    K.A. Knizhnik * / [] \ */
/*                          Last update: 27-Aug-98    K.A. Knizhnik * GARRET */
/*------------------------------------------------------------------*--------*/
/* Implementation of realtime timer and clock                       *        */
/*------------------------------------------------------------------*--------*/

#include <stdlib.h>
#include <alloca.h>
#include <assert.h>
#include <errno.h>

#include "posix_time.h"

#define TIMER_RESOLUTION  10000 /* 10ms - current resolution of Linux timer */

typedef struct timer { 
    struct timer*   next_active;
    struct timer*   next_expired;
    struct sigevent event;
    struct timeval  interval;
    struct timeval  corrected_interval;
    struct timeval  next_alarm;
    int             ticks;
    int             correction_period;
    int             running;
} timer;


static timer* active_timers;
static int    single_timer;    
static int    in_handler;
static struct itimerval timeout;
static void*  signal_handler[64];

static struct timeval resolution = { 0, TIMER_RESOLUTION }; 
static struct timeval half_resolution = { 0, TIMER_RESOLUTION/2 }; 

int timer_max_error = 1000000; /* 1 second */

#ifdef _REENTRANT
#include <pthread.h>
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#define lock()   pthread_mutex_lock(&mutex)
#define unlock() pthread_mutex_unlock(&mutex)
#else
#define lock()
#define unlock()
#endif

int timer_create(clockid_t clock, struct sigevent* spec, timer_t* timer_hdr)
{
    timer* t;

    if (clock != CLOCK_REALTIME || spec == NULL 
	|| (spec->sigev_notify != SIGEV_SIGNAL 
	    && spec->sigev_notify != SIGEV_NONE)) 
    { 
	errno = EINVAL;
	return -1;
    }
    t = (timer*)malloc(sizeof(timer));
    if (t == NULL) { 
	errno = ENOMEM;
	return -1;
    }
    t->event = *spec;
    t->running = 0;
    *timer_hdr = (timer_t)t;
    return 0;
}


static void notify(struct sigevent* event, void* ctx) 
{
    if (event->sigev_notify == SIGEV_SIGNAL) { 
	siginfo_t si;
	int signo = event->sigev_signo;
	si.si_value = event->sigev_value;
	(*(sa_handler_t)signal_handler[signo])(signo, &si, ctx);
    }
}


static void timer_handler(int signo)
{
    void* ctx = &signo + 1;
    struct timeval curr_time;
    struct timeval delta;
    struct itimerval itv;
    in_handler = 1;

    if (single_timer) { 
	timer* t = active_timers;
	assert(t != NULL && t->next_active == NULL);
	if (--t->ticks == 0) { 
	    t->ticks = t->correction_period;
	    gettimeofday(&curr_time, NULL);    
	    if (compare_time(&t->next_alarm, &curr_time) < 0) { 
		delta = curr_time;
		subtract_time(&delta, &t->next_alarm);
		if (delta.tv_sec*1000000+delta.tv_usec > timer_max_error) { 
                    if (compare_time(&t->corrected_interval, &resolution) > 0)
                    {
                        subtract_time(&t->corrected_interval, &resolution);
                    }
		    itv.it_value = itv.it_interval = t->corrected_interval;
		    setitimer(ITIMER_REAL, &itv, NULL);
		}
	    } else { 
		delta = t->next_alarm;
		subtract_time(&delta, &curr_time);
		if (delta.tv_sec*1000000+delta.tv_usec > timer_max_error) { 
		    add_time(&t->corrected_interval, &resolution);
		    itv.it_value = itv.it_interval = t->corrected_interval;
		    setitimer(ITIMER_REAL, &itv, NULL);
		}
	    }
	}
	add_time(&t->next_alarm, &t->interval);
	notify(&t->event, ctx);
    } else { 
	timer *t, **tp, *expired = NULL, *next_expired;
	struct timeval curr_rounded_time;
	
	timeout.it_value.tv_sec = 0;
	timeout.it_value.tv_usec = 0;
	timeout.it_interval.tv_sec = 0;
	timeout.it_interval.tv_usec = 0;
	gettimeofday(&curr_time, NULL);
	curr_rounded_time = curr_time;
	add_time(&curr_rounded_time, &half_resolution);

	for (tp = &active_timers; (t = *tp) != NULL; tp = &t->next_active) { 
	    if (compare_time(&curr_rounded_time, &t->next_alarm) >= 0) { 
		if (t->interval.tv_sec != 0 || t->interval.tv_usec != 0) { 
		    add_time(&t->next_alarm, &t->interval);
		    if (compare_time(&t->next_alarm, &curr_time) > 0) { 
			delta = t->next_alarm;
			subtract_time(&delta, &curr_time);
		    } else { 
			delta.tv_sec = 0;
			delta.tv_usec = TIMER_RESOLUTION;
		    }
		    if ((timeout.it_value.tv_sec == 0 
			 && timeout.it_value.tv_usec == 0)
			|| compare_time(&delta, &timeout.it_value) < 0)
		    {
			timeout.it_value = delta;
		    }
		} else { 
		    *tp = t->next_active;
		    t->running = 0;
		}
		t->next_expired = expired;
		expired = t;
	    } else { 
		struct timeval delta = t->next_alarm;
		subtract_time(&delta, &curr_time);
		if ((timeout.it_value.tv_sec == 0 
		     && timeout.it_value.tv_usec == 0)
		    || compare_time(&delta, &timeout.it_value) < 0)
		{
		    timeout.it_value = delta;
		}
	    }
	}
	for (t = expired; t != NULL; t = next_expired) { 
            next_expired = t->next_expired;
	    notify(&t->event, ctx);
	}
	if (!single_timer) { 
	    setitimer(ITIMER_REAL, &timeout, NULL);
	}
    }
    in_handler = 0;
}

int timer_settime(timer_t timer_hdr, int flags, struct itimerspec* val,
		  struct itimerspec* old)
{
    struct timeval  curr_time;
    struct itimerval it_new, it_old;
    timer* t = (timer*)timer_hdr;
    int save_mask;
    static struct sigaction new_sa;
    struct sigaction old_sa;
    new_sa.sa_handler = timer_handler;
    if (flags != TIMER_ABSTIME && flags != TIMER_RELTIME) { 
	errno = EINVAL;
	return -1;
    }
    if (val == NULL) {
	errno = EFAULT;
	return -1;
    }
    
    save_mask = sigblock(sigmask(SIGALRM));
    lock();
    sigaction(SIGALRM, &new_sa, &old_sa);
    if (old_sa.sa_handler != timer_handler) { 
	signal_handler[SIGALRM] = old_sa.sa_handler; 
    }
    if (t->event.sigev_signo != SIGALRM) {
	sigaction(t->event.sigev_signo, NULL, &old_sa);
	signal_handler[t->event.sigev_signo] = old_sa.sa_handler; 
    }
    if (old != NULL) { 
	old->it_interval.tv_sec = t->interval.tv_sec;
	old->it_interval.tv_nsec = t->interval.tv_usec*1000;
    }
    t->interval.tv_sec = val->it_interval.tv_sec;
    t->interval.tv_usec = val->it_interval.tv_nsec/1000;
    t->corrected_interval = t->interval;
    it_new.it_value.tv_sec = val->it_value.tv_sec;
    it_new.it_value.tv_usec = val->it_value.tv_nsec/1000;

    gettimeofday(&curr_time, NULL);

    if (flags == TIMER_ABSTIME) { 
	subtract_time(&it_new.it_value, &curr_time);
    }

    if (active_timers == NULL || (active_timers == t && !t->next_active)) { 
	it_new.it_interval.tv_sec = t->interval.tv_sec;
	it_new.it_interval.tv_usec = t->interval.tv_usec;
	setitimer(ITIMER_REAL, &it_new, &it_old);
	if (old != NULL) { 
	    if (!t->running) { 
		old->it_value.tv_sec = 0;
		old->it_value.tv_nsec = 0;
	    } else if (single_timer) { 
		old->it_value.tv_sec = it_old.it_value.tv_sec;
		old->it_value.tv_nsec = it_old.it_value.tv_usec*1000;
	    } else { 
		struct timeval remaining = t->next_alarm;
		if (compare_time(&curr_time, &remaining) < 0) { 
		    subtract_time(&remaining, &curr_time);
		    old->it_value.tv_sec = remaining.tv_sec;
		    old->it_value.tv_nsec = remaining.tv_usec*1000;
		} else { 
		    old->it_value.tv_sec = 0;
		    old->it_value.tv_nsec = 0;
		}
	    }
	}
	t->ticks = t->correction_period = timer_max_error/TIMER_RESOLUTION;
	single_timer = 1;
    } else { 
	if (in_handler) { 
	    assert(!single_timer);
	    if ((timeout.it_value.tv_sec == 0 
		 && timeout.it_value.tv_usec == 0) 
		|| ((it_new.it_value.tv_sec != 0 
		     || it_new.it_value.tv_usec != 0)
		    && compare_time(&it_new.it_value, &timeout.it_value) < 0))
	    {
		timeout.it_value = it_new.it_value;
	    }
	} else { 	
	    timer* nt;
	    single_timer = 0;
	    timeout.it_value = it_new.it_value;
	    timeout.it_interval.tv_sec = 0;
	    timeout.it_interval.tv_usec = 0;
	    for (nt = active_timers; nt != NULL; nt = nt->next_active) { 
		if (nt != t && compare_time(&nt->next_alarm, &curr_time) > 0) {
		    struct timeval remaining = nt->next_alarm;
		    subtract_time(&remaining, &curr_time);
		    if ((timeout.it_value.tv_sec == 0
			 && timeout.it_value.tv_usec == 0)
			|| compare_time(&remaining, &timeout.it_value) < 0)
		    {
			timeout.it_value = remaining;
		    }
		}
	    }
	    setitimer(ITIMER_REAL, &timeout, NULL);
	}
	if (old) { 
	    if (!t->running || compare_time(&t->next_alarm, &curr_time) <= 0) {
		old->it_value.tv_sec = 0;
		old->it_value.tv_nsec = 0;
	    } else {
		struct timeval remaining = t->next_alarm;
		subtract_time(&remaining, &curr_time);
		old->it_value.tv_sec = remaining.tv_sec;
		old->it_value.tv_nsec = remaining.tv_usec*1000;
	    }		
	} 
    } 
    t->next_alarm = curr_time;
    add_time(&t->next_alarm, &it_new.it_value);

    if (!t->running) {  
	if (it_new.it_value.tv_sec != 0 || it_new.it_value.tv_usec != 0) { 
	    t->next_active = active_timers;
	    active_timers = t;
	    t->running = 1;
	}
    } else { 
	if (it_new.it_value.tv_sec == 0 && it_new.it_value.tv_usec == 0) { 
	    timer** tp;
	    for (tp = &active_timers; *tp != t; tp = &(*tp)->next_active) { 
		if (*tp == NULL) { 
		    errno = EINVAL;
		    unlock();
		    sigsetmask(save_mask);
		    return -1;
		}
	    }
	    *tp = t->next_active;
	    t->running = 0;
	    single_timer = 0;
	}
    } 
    unlock();
    sigsetmask(save_mask);
    return 0;
}

int timer_gettime(timer_t timer_hdr, struct itimerspec* old)
{
    if (old) { 
	timer* t = (timer*)timer_hdr;
	old->it_interval.tv_sec = t->interval.tv_sec;
	old->it_interval.tv_nsec = t->interval.tv_usec*1000;
	if (!t->running) { 
	    old->it_value.tv_sec = 0;
	    old->it_value.tv_nsec = 0;
	} else { 
	    if (single_timer) { 
		struct itimerval it_old;    
		getitimer(ITIMER_REAL, &it_old);
		old->it_value.tv_sec = it_old.it_value.tv_sec;
		old->it_value.tv_nsec = it_old.it_value.tv_usec*1000;
	    } else { 
		struct timeval curr_time;
		gettimeofday(&curr_time, NULL);
		if (compare_time(&curr_time, &t->next_alarm) >= 0) { 
		    old->it_value.tv_sec = t->interval.tv_sec;
		    old->it_value.tv_nsec = t->interval.tv_usec*1000;
		} else { 
		    struct timeval delta = t->next_alarm;
		    subtract_time(&delta, &curr_time);
		    old->it_value.tv_sec = delta.tv_sec;
		    old->it_value.tv_nsec = delta.tv_usec*1000;
		}
	    }
	}		
    }
    return 0;
}

int timer_getoverrun(timer_t timer_hdr) 
{
    return 0;
}

int timer_delete(timer_t timer_hdr)
{
    int save_mask = sigblock(sigmask(SIGALRM));
    timer **tp, *t = (timer*)timer_hdr;
    lock();
    if (t->running) {
	for (tp = &active_timers; *tp != t; tp = &(*tp)->next_active) { 
	    if (*tp == NULL) { 
		errno = EINVAL;
		unlock();
		sigsetmask(save_mask);
		return -1;
	    }
	}
	*tp = t->next_active;
	single_timer = 0;
    }
    free(t);
    unlock();
    sigsetmask(save_mask);
    return 0;
}

int clock_gettime(clockid_t clock, struct timespec* val)
{
    if (clock != CLOCK_REALTIME || val == NULL) { 
	errno = EINVAL;
	return -1;
    } else { 
	struct timeval curr_time;
	gettimeofday(&curr_time, NULL);
	val->tv_sec = curr_time.tv_sec;
	val->tv_nsec = curr_time.tv_usec*1000;
	return 0;
    }
}

int clock_settime(clockid_t clock, struct timespec* val)
{
    if (clock != CLOCK_REALTIME || val == NULL) { 
	errno = EINVAL;
	return -1;
    } else { 
	struct timeval new_time;
	new_time.tv_sec = val->tv_sec;
	new_time.tv_usec = val->tv_nsec/1000;
	return settimeofday(&new_time, NULL);
    }
}

int clock_getres(clockid_t clock, struct timespec* resolution)
{
    if (clock != CLOCK_REALTIME || resolution == NULL) { 
	errno = EINVAL;
	return -1;
    }
    resolution->tv_sec = 0;
    resolution->tv_nsec = TIMER_RESOLUTION*1000; 
    return 0;
}



int compare_time(struct timeval* t1, struct timeval* t2)
{
    return t1->tv_sec < t2->tv_sec ? -1 :
	t1->tv_sec > t2->tv_sec ? 1 : t1->tv_usec - t2->tv_usec;
	
}

void add_time(struct timeval* t1, struct timeval* t2)
{
    t1->tv_sec += t2->tv_sec;
    t1->tv_usec += t2->tv_usec;
    if (t1->tv_usec > 1000000) { 
	t1->tv_sec += 1;
	t1->tv_usec -= 1000000;
    }
}

void subtract_time(struct timeval* t1, struct timeval* t2) 
{
    t1->tv_sec -= t2->tv_sec;
    t1->tv_usec -= t2->tv_usec;
    if (t1->tv_usec < 0) { 
	t1->tv_sec -= 1;
	t1->tv_usec += 1000000;
    }
}



