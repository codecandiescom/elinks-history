/* $Id: leds.h,v 1.6 2003/08/01 15:26:02 jonas Exp $ */

#ifndef EL__BFU_LEDS_H
#define EL__BFU_LEDS_H

#include "terminal/terminal.h"

/* TODO: Variable count! */
#define LEDS_COUNT	5

/* We use struct in order to at least somehow 'authorize' client to use certain
 * LED, preventing possible mess i.e. with conflicting patches or Lua scripts.
 */

/* See header of bfu/leds.c for LEDs assignment. If you are planning to use
 * some LED in your script/patch, please tell us on the list so that we can
 * register the LED for you. Always check latest sources for actual LED
 * assignment scheme in order to prevent conflicts. */

struct led {
	int number;
	unsigned char value;

	/* Use find_nearest_color() to set up color. Note that you shouldn't
	 * use color only as additional indication, as the terminal can be
	 * monochrome. */
	int color;

	/* Private data. */
	int __used;
};

void init_leds(void);
void done_leds(void);
void draw_leds(struct terminal *);

struct led *register_led(int);
void unregister_led(struct led *);

#endif
