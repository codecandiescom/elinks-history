/* $Id: timers.h,v 1.2 2005/03/04 14:01:06 jonas Exp $ */

#ifndef EL__LOWLEVEL_TIMERS_H
#define EL__LOWLEVEL_TIMERS_H

#include "util/ttime.h"

int count_timers(void);
void check_timers(ttime *last_time);
int install_timer(ttime, void (*)(void *), void *);
void kill_timer(int);
int get_next_timer_time(struct timeval *tv);

#endif
