/* Timers. */
/* $Id: timer.c,v 1.13 2005/04/14 10:46:12 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "lowlevel/select.h"
#include "lowlevel/timers.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/ttime.h"


struct timer {
	LIST_HEAD(struct timer);

	double interval;
	void (*func)(void *);
	void *data;
};

static INIT_LIST_HEAD(timers);

int
count_timers(void)
{
	return list_size(&timers);
}

void
check_timers(time_T *last_time)
{
	time_T now = get_time();
	time_T interval = now - *last_time;
	struct timer *timer;

	foreach (timer, timers) timer->interval -= interval;

	while (!list_empty(timers)) {
		timer = timers.next;

		if (timer->interval > 0) break;

		del_from_list(timer);
		timer->func(timer->data);
		mem_free(timer);
		check_bottom_halves();
	}

	*last_time = now;
}

void
install_timer(timer_id_T *id, long int delay_in_milliseconds, void (*func)(void *), void *data)
{
	double delay_in_seconds = (double) delay_in_milliseconds / 1000.0;
	struct timer *new_timer, *timer;

	assert(id && delay_in_seconds > 0);

	new_timer = mem_alloc(sizeof(*new_timer));
	*id = (timer_id_T) new_timer; /* TIMER_ID_UNDEF is NULL */
	if (!new_timer) return;

	new_timer->interval = delay_in_seconds;
	new_timer->func = func;
	new_timer->data = data;

	foreach (timer, timers)
		if (timer->interval >= delay_in_seconds)
			break;
	add_at_pos(timer->prev, new_timer);
}

void
kill_timer(timer_id_T *id)
{
	struct timer *timer;

	assert(id != NULL);
	if (*id == TIMER_ID_UNDEF) return;

	timer = *id;
	del_from_list(timer);
	mem_free(timer);

	*id = TIMER_ID_UNDEF;
}

int
get_next_timer_time(struct timeval *tv)
{
	if (!list_empty(timers)) {
		time_T tt = ((struct timer *) &timers)->next->interval;

		if (tt < 0) tt = 0;
		tv->tv_sec = tt / 1000;
		tv->tv_usec = (tt % 1000) * 1000;

		return 1;
	}

	return 0;
}
