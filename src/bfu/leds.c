/* These cute LightEmittingDiode-like indicators. */
/* $Id: leds.c,v 1.2 2002/07/06 21:53:00 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "bfu/colors.h"
#include "bfu/leds.h"
#include "lowlevel/select.h"
#include "lowlevel/terminal.h"
#include "util/error.h"


/* Current leds allocation:
 * 1 - unused, reserved for ELinks internal use
 * 2 - unused, reserved for ELinks internal use
 * 3 - unsued, reserved for Lua
 * 4 - unused, reserved for Lua
 * 5 - unused */

/* Always reset led to '-' when not used anymore. */

/* If we would do real protection, we would do this as array of pointers. This
 * way someone can just get any struct led and add/subscribe appropriate struct
 * led for his control; however, I bet on programmers' responsibility rather,
 * and hope that everyone will abide the "rules". */
static struct led leds[LEDS_COUNT];
static unsigned char leds_backup[LEDS_COUNT];

static int redraw_timer;


void redraw_leds(void *);

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

	/* Redraw each 100ms. */
	redraw_timer = install_timer(100, redraw_leds, NULL);
}

void
done_leds()
{
	kill_timer(redraw_timer);
}

void
draw_leds(struct terminal *term)
{
	int i;

	/* We must shift the whole thing by one char to left, because we don't
	 * draft the char in the right-down corner :(. */

	set_char(term, term->x - LEDS_COUNT - 3, term->y - 1, '[' | COL(070));

	for (i = 0; i < LEDS_COUNT; i++)
		set_char(term, term->x - LEDS_COUNT - 2 + i, term->y - 1,
			 leds[i].value | leds[i].color);

	set_char(term, term->x - 2, term->y - 1, ']' | COL(070));
}

/* Determine if leds redrawing if neccessary. Returns non-zero if so. */
int
sync_leds()
{
	int resync = 0;
	int i;

	for (i = 0; i < LEDS_COUNT; i++) {
		if (leds[i].value != leds_backup[i]) {
			leds_backup[i] = leds[i].value;
			resync++;
		}
	}

	return resync;
}

void
redraw_leds(void *xxx)
{
	struct terminal *term;

	redraw_timer = install_timer(100, redraw_leds, NULL);

	if (!sync_leds()) return;

	foreach (term, terminals) {
		redraw_terminal(term);
		draw_leds(term);
	}
}


struct led *
register_led(int number)
{
	if (number >= LEDS_COUNT || leds[number].__used) return NULL;
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
