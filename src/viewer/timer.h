/* $Id: timer.h,v 1.3 2005/03/04 10:21:12 zas Exp $ */

#ifndef EL__LOWLEVEL_TIMER_H
#define EL__LOWLEVEL_TIMER_H

#include "util/lists.h"
#include "util/ttime.h"

extern int timer_duration;

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

void reset_timer(void);
void init_timer(void);
void done_timer(void);

#endif
