/* $Id: leds.h,v 1.9 2003/10/26 14:30:51 jonas Exp $ */

#ifndef EL__BFU_LEDS_H
#define EL__BFU_LEDS_H

#include "modules/module.h"
#include "terminal/terminal.h"
#include "util/color.h"

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

	/* The led's foreground color. Note that you should use color only as
	 * additional indication, as the terminal can be monochrome. */
	color_t fgcolor;

	/* Private data. */
	int __used;
};

extern struct module leds_module;

void draw_leds(struct terminal *);

struct led *register_led(int);
void unregister_led(struct led *);

#endif
