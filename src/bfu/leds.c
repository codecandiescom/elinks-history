/* These cute LightEmittingDiode-like indicators. */
/* $Id: leds.c,v 1.14 2003/05/04 19:30:47 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_LEDS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/leds.h"
#include "config/options.h"
#include "lowlevel/timer.h"
#include "lowlevel/select.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/error.h"


/* Current leds allocation:
 * 1 - unused, reserved for ELinks internal use
 * 2 - unused, reserved for ELinks internal use
 * 3 - unused, reserved for Lua
 * 4 - unused, reserved for Lua
 * 5 - unused */

/* Always reset led to '-' when not used anymore. */

/* If we would do real protection, we would do this as array of pointers. This
 * way someone can just get any struct led and add/subscribe appropriate struct
 * led for his control; however, I bet on programmers' responsibility rather,
 * and hope that everyone will abide the "rules". */

/* TODO: In order for this to have some real value, this should be per-session,
 * not global. Then we could even use it for something ;-). --pasky */

static struct led leds[LEDS_COUNT];
static unsigned char leds_backup[LEDS_COUNT];
static int timer_duration_backup = 0;

static int redraw_timer = -1;
static int drawing = 0;


static void redraw_leds(void *);

void
init_leds()
{
	int i;

	for (i = 0; i < LEDS_COUNT; i++) {
		leds[i].number = i;
		leds[i].value = '-';
		leds[i].color = COL(070);
		leds[i].__used = 0;
		leds_backup[i] = 0; /* assure first redraw */
	}

	timer_duration_backup = 0;

	/* We can't setup timer here, because we may not manage to startup in
	 * 100ms and we will get to problems when we will call draw_leds() on
	 * uninitialized terminal. So, we will wait for draw_leds(). */
}

void
done_leds()
{
	if (redraw_timer >= 0) kill_timer(redraw_timer);
}

void
draw_leds(struct terminal *term)
{
	int i;

	/* This should be done elsewhere, but this is very nice place where we
	 * could do that easily. */
	if (get_opt_int("ui.timer.enable") == 2) {
		char s[256]; int l;

		snprintf(s, 256, "[%d]", timer_duration);
		l = strlen(s);

		for (i = l - 1; i >= 0; i--)
			set_char(term, term->x - LEDS_COUNT - 3 - (l - i),
				 term->y - 1, s[i] | COL(070));
	}

	/* We must shift the whole thing by one char to left, because we don't
	 * draft the char in the right-down corner :(. */

	set_char(term, term->x - LEDS_COUNT - 3, term->y - 1, '[' | COL(070));

	for (i = 0; i < LEDS_COUNT; i++)
		set_char(term, term->x - LEDS_COUNT - 2 + i, term->y - 1,
			 leds[i].value | leds[i].color);

	set_char(term, term->x - 2, term->y - 1, ']' | COL(070));

	/* Redraw each 100ms. */
	if (!drawing && redraw_timer < 0)
		redraw_timer = install_timer(100, redraw_leds, NULL);
}

/* Determine if leds redrawing if neccessary. Returns non-zero if so. */
static int
sync_leds()
{
	int resync = 0;
	int i;

	if (timer_duration_backup != timer_duration) {
		timer_duration_backup = timer_duration;
		resync++;
	}

	for (i = 0; i < LEDS_COUNT; i++) {
		if (leds[i].value != leds_backup[i]) {
			leds_backup[i] = leds[i].value;
			resync++;
		}
	}

	return resync;
}

static void
redraw_leds(void *xxx)
{
	struct terminal *term;

	redraw_timer = install_timer(100, redraw_leds, NULL);

	if (drawing) return;
	drawing = 1;

	if (!sync_leds()) {
		drawing = 0;
		return;
	}

	foreach (term, terminals) {
		redraw_terminal(term);
		draw_leds(term);
	}
	drawing = 0;
}


struct led *
register_led(int number)
{
	if (number >= LEDS_COUNT || number < 0 || leds[number].__used)
		return NULL;
	return &leds[number];
}

void
unregister_led(struct led *led)
{
	if (!led->__used)
		internal("Attempted to unregister unused led!");
	led->__used = 0;
	led->value = '-';
	led->color = COL(070);
}

#endif
