/* Timers. */
/* $Id: timer.c,v 1.3 2005/03/04 17:36:29 zas Exp $ */

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

	ttime interval;
	void (*func)(void *);
	void *data;
	timer_id_T id;
};

static INIT_LIST_HEAD(timers);

int
count_timers(void)
{
	return list_size(&timers);
}

void
check_timers(ttime *last_time)
{
	ttime now = get_time();
	ttime interval = now - *last_time;
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

timer_id_T
install_timer(ttime time, void (*func)(void *), void *data)
{
	static timer_id_T timer_id = 0;
	struct timer *new_timer, *timer;

	new_timer = mem_alloc(sizeof(*new_timer));
	if (!new_timer) return TIMER_ID_UNDEF;
	new_timer->interval = time;
	new_timer->func = func;
	new_timer->data = data;
	new_timer->id = timer_id++;
	foreach (timer, timers)
		if (timer->interval >= time)
			break;
	add_at_pos(timer->prev, new_timer);

	return new_timer->id;
}

void
kill_timer(timer_id_T id)
{
	struct timer *timer, *next;
	int k = 0;

	foreachsafe (timer, next, timers)
		if (timer->id == id) {
			del_from_list(timer);
			mem_free(timer);
			k++;
		}

	assertm(k, "trying to kill nonexisting timer");
	assertm(k < 2, "more timers with same id");
}

int
get_next_timer_time(struct timeval *tv)
{
	if (!list_empty(timers)) {
		ttime tt = ((struct timer *) &timers)->next->interval + 1;

		if (tt < 0) tt = 0;
		tv->tv_sec = tt / 1000;
		tv->tv_usec = (tt % 1000) * 1000;

		return 1;
	}

	return 0;
}
