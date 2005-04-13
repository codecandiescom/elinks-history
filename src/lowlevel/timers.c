/* Timers. */
/* $Id: timers.c,v 1.9 2005/04/13 13:06:40 zas Exp $ */

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

	struct timeval interval;
	void (*func)(void *);
	void *data;
};

static INIT_LIST_HEAD(timers);

int
count_timers(void)
{
	return list_size(&timers);
}

static void
timeval_sub(struct timeval *res, struct timeval *a, struct timeval *b)
{
	res->tv_sec = a->tv_sec - b->tv_sec;
	res->tv_usec = a->tv_usec - b->tv_usec;

	while (res->tv_usec < 0) {
		res->tv_usec += 1000000;
		res->tv_sec--;
	}
}

static int
positive_timeval(struct timeval *a)
{
	return (a->tv_sec > 0 || (a->tv_sec == 0 && a->tv_usec > 0));
}

static void
milliseconds_to_timeval(struct timeval *a, int milliseconds)
{
	a->tv_sec = milliseconds / 1000;
	a->tv_usec = (milliseconds % 1000) * 1000;
}

void
check_timers(struct timeval *last_time)
{
	struct timeval now;
	struct timeval interval;
	struct timer *timer;

	gettimeofday(&now, NULL);

	timeval_sub(&interval, &now, last_time);
	foreach (timer, timers) {
		timeval_sub(&timer->interval, &timer->interval, &interval);
	}
	
	while (!list_empty(timers)) {
		timer = timers.next;

		if (positive_timeval(&timer->interval))
			break;

		del_from_list(timer);
		timer->func(timer->data);
		mem_free(timer);
		check_bottom_halves();
	}

	copy_struct(last_time, &now);
}

void
install_timer(timer_id_T *id, int delay, void (*func)(void *), void *data)
{
	struct timer *new_timer, *timer;

	assert(id && delay > 0);

	new_timer = mem_alloc(sizeof(*new_timer));
	*id = (timer_id_T) new_timer; /* TIMER_ID_UNDEF is NULL */
	if (!new_timer) return;

	milliseconds_to_timeval(&new_timer->interval, delay);
	new_timer->func = func;
	new_timer->data = data;
	
	foreach (timer, timers) {
		struct timeval d;

		timeval_sub(&d, &timer->interval, &new_timer->interval);
		
		if (positive_timeval(&d))
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
get_next_timer_time(struct timeval *tv)
{
	if (!list_empty(timers)) {
		copy_struct(tv, &((struct timer *) &timers)->next->interval);
		
		if (!positive_timeval(tv)) {
			tv->tv_sec = 0;
			tv->tv_usec = 1000;	/* Minimal delay, in case of... */
		}

		return 1;
	}

	return 0;
}
