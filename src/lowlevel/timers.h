/* $Id: timers.h,v 1.1 2005/03/04 13:19:37 zas Exp $ */

#ifndef EL__LOWLEVEL_TIMERS_H
#define EL__LOWLEVEL_TIMERS_H

#include "util/lists.h"
#include "util/ttime.h"

struct timer {
	LIST_HEAD(struct timer);

	ttime interval;
	void (*func)(void *);
	void *data;
	int id;
};

int count_timers(void);
void check_timers(ttime *last_time);
int install_timer(ttime, void (*)(void *), void *);
void kill_timer(int);
int get_next_timer_time(struct timeval *tv);

#endif
