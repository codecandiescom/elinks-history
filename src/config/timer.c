/* Periodic saving module */
/* $Id: timer.c,v 1.3 2005/08/25 15:08:00 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "config/timer.h"
#include "main/event.h"
#include "main/module.h"
#include "main/timer.h"
#include "util/time.h"


/* Timer for periodically saving configuration files to disk */
static timer_id_T periodic_save_timer = TIMER_ID_UNDEF;

static void
periodic_save_handler(void *xxx)
{
	static int periodic_save_event_id = EVENT_NONE;
	milliseconds_T interval;

	if (get_cmd_opt_bool("anonymous")) return;

	/* Don't trigger anything at startup */
	if (periodic_save_event_id == EVENT_NONE)
		set_event_id(periodic_save_event_id, "periodic-saving");
	else
		trigger_event(periodic_save_event_id);

	interval = (milliseconds_T) (get_opt_int("infofiles.save_interval") * 1000);
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
}

static void
done_timer(struct module *module)
{
	kill_timer(&periodic_save_timer);
}

struct module periodic_saving_module = struct_module(
	/* name: */		"Periodic Saving",
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_timer,
	/* done: */		done_timer
);
