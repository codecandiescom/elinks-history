/* Internal inactivity timer. */
/* $Id: timer.c,v 1.22 2005/04/07 11:16:57 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/kbdbind.h"
#include "config/options.h"
#include "lowlevel/select.h"
#include "lowlevel/timer.h"
#include "lowlevel/timers.h"
#include "modules/module.h"
#include "sched/event.h"
#include "terminal/event.h"
#include "terminal/terminal.h"


/* Timer for periodically saving configuration files to disk */
static timer_id_T periodic_save_timer = TIMER_ID_UNDEF;

static timer_id_T countdown = TIMER_ID_UNDEF;

static int timer_duration = 0;

int
get_timer_duration(void)
{
	return timer_duration;
}

static void
count_down(void *xxx)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_KBD, -1, 0, 0);
	struct keybinding *kb;
	struct terminal *terminal;

	timer_duration--;
	if (timer_duration) {
		install_timer(&countdown, 1000, count_down, NULL);
		return;
	} else {
		countdown = TIMER_ID_UNDEF;
	}

	kb = kbd_nm_lookup(KEYMAP_MAIN, get_opt_str("ui.timer.action"));
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
	kill_timer(&countdown);

	if (!get_opt_int("ui.timer.enable")) return;

	timer_duration = get_opt_int("ui.timer.duration");
	install_timer(&countdown, 1000, count_down, NULL);
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

	install_timer(&periodic_save_timer, interval, periodic_save_handler, NULL);
}

static int
periodic_save_change_hook(struct session *ses, struct option *current,
			  struct option *changed)
{
	if (get_cmd_opt_bool("anonymous")) return 0;

	kill_timer(&periodic_save_timer);

	periodic_save_handler(NULL);

	return 0;
}

static void
init_timer(struct module *module)
{
	struct change_hook_info timer_change_hooks[] = {
		{ "infofiles.save_interval", periodic_save_change_hook },
		{ NULL,	NULL },
	};

	register_change_hooks(timer_change_hooks);
	periodic_save_handler(NULL);
	reset_timer();
}

static void
done_timer(struct module *module)
{
	kill_timer(&periodic_save_timer);
	kill_timer(&countdown);
}

struct module timer_module = struct_module(
	/* name: */		"Timer",
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_timer,
	/* done: */		done_timer
);
