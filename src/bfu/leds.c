/* These cute LightEmittingDiode-like indicators. */
/* $Id: leds.c,v 1.40 2004/01/01 14:03:51 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_LEDS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/leds.h"
#include "bfu/style.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "lowlevel/timer.h"
#include "modules/module.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/error.h"


/* Current leds allocation:
 * 1 - SSL connection indicator
 * 2 - unused, reserved for ELinks internal use
 * 3 - unused, reserved for Lua
 * 4 - unused, reserved for Lua
 * 5 - unused */

/* XXX: Currently, the leds toggling is quite hackish, some more work should go
 * to it (ie. some led hooks called in sync_leds() to light the leds
 * dynamically. --pasky */

/* Always reset led to '-' when not used anymore. */

/* If we would do real protection, we would do this as array of pointers. This
 * way someone can just get any struct led and add/subscribe appropriate struct
 * led for his control; however, I bet on programmers' responsibility rather,
 * and hope that everyone will abide the "rules". */

static int timer_duration_backup = 0;

static int redraw_timer = -1;
static int drawing = 0;

static void redraw_leds(void *);

enum led_option {
	LED_TREE,

	LEDS_ENABLE,

	LEDS_OPTIONS,
};

static struct option_info led_options[] = {
	INIT_OPT_TREE("ui", N_("LEDs"),
		"leds", 0,
		N_("LEDs (visual indicators) options.")),

	INIT_OPT_BOOL("ui.leds", N_("Enable"),
		"enable", 0, 1,
		N_("Enable LEDs.\n"
		   "These visual indicators will inform you about various states.")),

	NULL_OPTION_INFO,
};

#define get_opt_leds(which)		led_options[(which)].option.value
#define get_leds_enable()		get_opt_leds(LEDS_ENABLE).number

void
init_leds(struct module *module)
{
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
init_led_panel(struct led_panel *leds)
{
	int i;

	for (i = 0; i < LEDS_COUNT; i++) {
		leds->leds[i].number = i;
		leds->leds[i].value = '-';
		leds->leds[i].__used = 0;
		leds->leds_backup[i] = 0; /* assure first redraw */
	}
}

void
draw_leds(struct session *ses)
{
	struct terminal *term = ses->tab->term;
	struct color_pair *led_color = NULL;
	int i;
	int xpos = term->width - LEDS_COUNT - 3;
	int ypos = term->height - 1;

	/* This should be done elsewhere, but this is very nice place where we
	 * could do that easily. */
	if (get_opt_int("ui.timer.enable") == 2) {
		char s[256]; int l;

		snprintf(s, 256, "[%d]", timer_duration);
		l = strlen(s);
		led_color = get_bfu_color(term, "status.status-text");
		if (!led_color) goto end;

		for (i = l - 1; i >= 0; i--)
			draw_char(term, xpos - (l - i), ypos, s[i], 0, led_color);
	}

	if (!get_leds_enable()) return;

	if (!led_color) {
		led_color = get_bfu_color(term, "status.status-text");
		if (!led_color) goto end;
	}

	/* We must shift the whole thing by one char to left, because we don't
	 * draft the char in the right-down corner :(. */

	draw_char(term, xpos, ypos, '[', 0,  led_color);

	for (i = 0; i < LEDS_COUNT; i++) {
		struct led *led = &ses->status.leds.leds[i];

		draw_char(term, xpos + i + 1, ypos, led->value, 0, led_color);
	}

	draw_char(term, xpos + LEDS_COUNT + 1, ypos, ']', 0, led_color);

end:
	/* Redraw each 100ms. */
	if (!drawing && redraw_timer < 0)
		redraw_timer = install_timer(100, redraw_leds, NULL);
}

/* Determine if leds redrawing is necessary. Returns non-zero if so. */
static int
sync_leds(struct session *ses)
{
	int resync = 0;
	int i;

	if (timer_duration_backup != timer_duration) {
		timer_duration_backup = timer_duration;
		resync++;
	}

	for (i = 0; i < LEDS_COUNT; i++) {
		struct led *led = &ses->status.leds.leds[i];
		unsigned char *led_backup = &ses->status.leds.leds_backup[i];

		if (led->value != *led_backup) {
			*led_backup = led->value;
			resync++;
		}
	}

	return resync;
}

static void
redraw_leds(void *xxx)
{
	struct session *ses;

	if (!get_leds_enable()
	    && get_opt_int("ui.timer.enable") != 2) {
		redraw_timer = -1;
		return;
	}

	redraw_timer = install_timer(100, redraw_leds, NULL);

	if (drawing) return;
	drawing = 1;

	foreach (ses, sessions) {
		if (!sync_leds(ses))
			continue;
		redraw_terminal(ses->tab->term);
		draw_leds(ses);
	}
	drawing = 0;
}


struct led *
register_led(struct session *ses, int number)
{
	if (number >= LEDS_COUNT || number < 0)
		return NULL;

	if (ses->status.leds.leds[number].__used)
		return NULL;

	ses->status.leds.leds[number].__used = 1;

	return &ses->status.leds.leds[number];
}

void
unregister_led(struct led *led)
{
	assertm(led->__used, "Attempted to unregister unused led!");
	led->__used = 0;
	led->value = '-';
}

struct module leds_module = struct_module(
	/* name: */		N_("LED indicators"),
	/* options: */		led_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_leds,
	/* done: */		done_leds
);

#endif
