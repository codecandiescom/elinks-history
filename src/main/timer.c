/* Timers. */
/* $Id: timer.c,v 1.21 2005/04/15 18:25:58 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "lowlevel/select.h"
#include "lowlevel/timers.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/time.h"
#include "util/types.h"	/* INFO_ type */


struct timer {
	LIST_HEAD(struct timer);

	timeval_T interval;
	void (*func)(void *);
	void *data;
};

static INIT_LIST_HEAD(timers);

long
timers_info(int type)
{
	switch (type) {
	case INFO_TIMERS:
		return list_size(&timers);

	default:
		return 0;
	}

}

void
check_timers(timeval_T *last_time)
{
	timeval_T now;
	timeval_T interval;
	struct timer *timer, *next;

	get_timeval(&now);
	timeval_sub(&interval, last_time, &now);

	foreach (timer, timers) {
		timeval_sub_interval(&timer->interval, &interval);
	}

	foreachsafe (timer, next, timers) {
		if (timeval_is_positive(&timer->interval))
			break;

		del_from_list(timer);
		timer->func(timer->data);
		mem_free(timer);
		check_bottom_halves();
	}

	copy_struct(last_time, &now);
}

void
install_timer(timer_id_T *id, long int delay_in_milliseconds, void (*func)(void *), void *data)
{
	struct timer *new_timer, *timer;

	assert(id && delay_in_milliseconds > 0);

	new_timer = mem_alloc(sizeof(*new_timer));
	*id = (timer_id_T) new_timer; /* TIMER_ID_UNDEF is NULL */
	if (!new_timer) return;

	milliseconds_to_timeval(&new_timer->interval, delay_in_milliseconds);
	new_timer->func = func;
	new_timer->data = data;

	foreach (timer, timers) {
		if (timeval_cmp(&timer->interval, &new_timer->interval) >= 0)
			break;
	}

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
get_next_timer_time(timeval_T *t)
{
	if (!list_empty(timers)) {
		copy_struct(t, &((struct timer *) &timers)->next->interval);
		return 1;
	}

	return 0;
}
