/* These cute LightEmittingDiode-like indicators. */
/* $Id: leds.c,v 1.23 2003/10/26 14:30:51 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_LEDS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/leds.h"
#include "bfu/style.h"
#include "config/options.h"
#include "lowlevel/select.h"
#include "lowlevel/timer.h"
#include "modules/module.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/color.h"
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
init_leds(struct module *module)
{
	int i;

	for (i = 0; i < LEDS_COUNT; i++) {
		leds[i].number = i;
		leds[i].value = '-';
		leds[i].__used = 0;
		leds_backup[i] = 0; /* assure first redraw */
	}

	timer_duration_backup = 0;

	/* We can't setup timer here, because we may not manage to startup in
	 * 100ms and we will get to problems when we will call draw_leds() on
	 * uninitialized terminal. So, we will wait for draw_leds(). */
}

void
done_leds(struct module *module)
{
	if (redraw_timer >= 0) kill_timer(redraw_timer);
}

void
draw_leds(struct terminal *term)
{
	/* We need a working copy because changing members of @led_color is
	 * bad since we might not be the only user. */
	struct color_pair color;
	struct color_pair *led_color;
	int i;

	led_color = get_bfu_color(term, "status.status-text");
	if (!led_color) {
		if (!drawing && redraw_timer < 0)
			redraw_timer = install_timer(100, redraw_leds, NULL);
		return;
	}

	/* This should be done elsewhere, but this is very nice place where we
	 * could do that easily. */
	if (get_opt_int("ui.timer.enable") == 2) {
		char s[256]; int l;

		snprintf(s, 256, "[%d]", timer_duration);
		l = strlen(s);

		for (i = l - 1; i >= 0; i--)
			draw_char(term, term->x - LEDS_COUNT - 3 - (l - i),
				  term->y - 1, s[i], 0, led_color);
	}

	/* We must shift the whole thing by one char to left, because we don't
	 * draft the char in the right-down corner :(. */

	draw_char(term, term->x - LEDS_COUNT - 3, term->y - 1,
		  '[', 0,  led_color);

	color.background = led_color->background;

	for (i = 0; i < LEDS_COUNT; i++) {
		color.foreground = leds[i].__used ? leds[i].fgcolor
						  : led_color->foreground;

		draw_char(term, term->x - LEDS_COUNT - 2 + i, term->y - 1,
			  leds[i].value, 0, &color);
	}

	draw_char(term, term->x - 2, term->y - 1, ']', 0, led_color);

	/* Redraw each 100ms. */
	if (!drawing && redraw_timer < 0)
		redraw_timer = install_timer(100, redraw_leds, NULL);
}

/* Determine if leds redrawing if necessary. Returns non-zero if so. */
static int
sync_leds(void)
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
	assertm(led->__used, "Attempted to unregister unused led!");
	led->__used = 0;
	led->value = '-';
}

struct module leds_module = struct_module(
	/* name: */		"leds",
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_leds,
	/* done: */		done_leds
);

#endif
