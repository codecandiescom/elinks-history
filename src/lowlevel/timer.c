/* Internal inactivity timer. */
/* $Id: timer.c,v 1.8 2003/09/25 19:32:17 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/kbdbind.h"
#include "config/options.h"
#include "lowlevel/select.h"
#include "lowlevel/timer.h"
#include "terminal/event.h"
#include "terminal/terminal.h"


static int countdown = -1;

int timer_duration = 0;

static void
count_down(void *xxx)
{
	struct term_event ev = INIT_TERM_EVENT(EV_KBD, -1, 0, 0);
	struct keybinding *kb;
	struct terminal *terminal;

	timer_duration--;
	if (timer_duration) {
		countdown = install_timer(1000, count_down, NULL);
		return;
	} else {
		countdown = -1;
	}

	kb = kbd_nm_lookup(KM_MAIN, get_opt_str("ui.timer.action"), NULL);
	if (kb) {
		ev.x = kb->key;
		ev.y = kb->meta;
		foreach (terminal, terminals) {
			term_send_event(terminal, &ev);
		}
	}

	reset_timer();
}

void
reset_timer(void)
{
	if (countdown >= 0) kill_timer(countdown);

	if (!get_opt_int("ui.timer.enable")) return;

	timer_duration = get_opt_int("ui.timer.duration");
	countdown = install_timer(1000, count_down, NULL);
}

void
init_timer(void)
{
	reset_timer();
}

void
done_timer(void)
{
	if (countdown >= 0) kill_timer(countdown);
}
