/* Internal inactivity timer. */
/* $Id: timer.c,v 1.15 2005/03/04 10:21:12 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/kbdbind.h"
#include "config/options.h"
#include "lowlevel/select.h"
#include "lowlevel/timer.h"
#include "sched/event.h"
#include "terminal/event.h"
#include "terminal/terminal.h"


/* Timer for periodically saving configuration files to disk */
static int periodic_save_timer = -1;

static int countdown = -1;

int timer_duration = 0;

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

int
install_timer(ttime time, void (*func)(void *), void *data)
{
	static int timer_id = 0;
	struct timer *new_timer, *timer;

	new_timer = mem_alloc(sizeof(*new_timer));
	if (!new_timer) return -1;
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
kill_timer(int id)
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

static void
count_down(void *xxx)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_KBD, -1, 0, 0);
	struct keybinding *kb;
	struct terminal *terminal;

	timer_duration--;
	if (timer_duration) {
		countdown = install_timer(1000, count_down, NULL);
		return;
	} else {
		countdown = -1;
	}

	kb = kbd_nm_lookup(KEYMAP_MAIN, get_opt_str("ui.timer.action"), NULL);
	if (kb) {
		ev.info.keyboard.key = kb->key;
		ev.info.keyboard.modifier = kb->meta;

		foreach (terminal, terminals) {
			term_send_event(terminal, &ev);
		}
	}

	reset_timer();
}

void
reset_timer(void)
{
	if (countdown >= 0) {
		kill_timer(countdown);
		countdown = -1;
	}

	if (!get_opt_int("ui.timer.enable")) return;

	timer_duration = get_opt_int("ui.timer.duration");
	countdown = install_timer(1000, count_down, NULL);
}


static void
periodic_save_handler(void *xxx)
{
	static int periodic_save_event_id = EVENT_NONE;
	int interval;

	if (get_cmd_opt_bool("anonymous")) return;

	/* Don't trigger anything at startup */
	if (periodic_save_event_id == EVENT_NONE)
		set_event_id(periodic_save_event_id, "periodic-saving");
	else
		trigger_event(periodic_save_event_id);

	interval = get_opt_int("infofiles.save_interval") * 1000;
	if (!interval) return;

	periodic_save_timer = install_timer(interval, periodic_save_handler, NULL);
}

static int
periodic_save_change_hook(struct session *ses, struct option *current,
			  struct option *changed)
{
	if (get_cmd_opt_bool("anonymous")) return 0;

	if (periodic_save_timer != -1) {
		kill_timer(periodic_save_timer);
		periodic_save_timer = -1;
	}

	periodic_save_handler(NULL);

	return 0;
}


void
init_timer(void)
{
	struct change_hook_info timer_change_hooks[] = {
		{ "infofiles.save_interval", periodic_save_change_hook },
		{ NULL,	NULL },
	};

	register_change_hooks(timer_change_hooks);
	periodic_save_handler(NULL);
	reset_timer();
}

void
done_timer(void)
{
	if (periodic_save_timer >= 0) kill_timer(periodic_save_timer);
	if (countdown >= 0) kill_timer(countdown);
}
