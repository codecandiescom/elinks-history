/* These cute LightEmittingDiode-like indicators. */
/* $Id: leds.c,v 1.1 2002/07/06 21:08:43 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "bfu/colors.h"
#include "bfu/leds.h"
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


void
setup_leds()
{
	int i;

	for (i = 0; i < LEDS_COUNT; i++) {
		leds[i].number = i;
		leds[i].value = '-';
		leds[i].color = COL(070);
		leds[i].__used = 0;
	}
}

void
draw_leds(struct terminal *term)
{
	int i;

	set_char(term, term->x - LEDS_COUNT - 2, term->y, '[' | COL(070));

	for (i = 0; i < LEDS_COUNT; i++)
		set_char(term, term->x - LEDS_COUNT - i, term->y,
			 leds[i].value | leds[i].color);

	set_char(term, term->x, term->y, ']' | COL(070));
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
