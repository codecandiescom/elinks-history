/* $Id: timer.h,v 1.8 2005/03/05 22:14:32 zas Exp $ */

#ifndef EL__LOWLEVEL_TIMERS_H
#define EL__LOWLEVEL_TIMERS_H

#include "util/ttime.h"

/* Little hack, timer_id_T is in fact a pointer to the timer, so
 * it has to be of a pointer type.
 * Using void * because we don't want to export struct timer.
 * The fact each timer is allocated ensure us that timer id will
 * be unique.
 * That way there is no need of id field in struct timer. --Zas */
typedef void * timer_id_T;

/* Should always be NULL or you'll have to modify install_timer()
 * and kill_timer(). --Zas */
#define TIMER_ID_UNDEF ((timer_id_T) NULL)

int count_timers(void);
void check_timers(time_T *last_time);
void install_timer(timer_id_T *id, time_T, void (*)(void *), void *);
void kill_timer(timer_id_T *id);
int get_next_timer_time(struct timeval *tv);

#endif
