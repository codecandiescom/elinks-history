/* $Id: timer.h,v 1.4 2005/03/04 17:55:36 zas Exp $ */

#ifndef EL__LOWLEVEL_TIMERS_H
#define EL__LOWLEVEL_TIMERS_H

#include "util/ttime.h"

typedef int timer_id_T;

#define TIMER_ID_UNDEF ((timer_id_T) -1)

int count_timers(void);
void check_timers(ttime *last_time);
timer_id_T install_timer(ttime, void (*)(void *), void *);
void kill_timer(timer_id_T *id);
int get_next_timer_time(struct timeval *tv);

#endif
