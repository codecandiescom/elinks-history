/* $Id: leds.h,v 1.13 2003/11/17 11:10:55 pasky Exp $ */

#ifndef EL__BFU_LEDS_H
#define EL__BFU_LEDS_H

#include "modules/module.h"
#include "util/color.h"

struct session;

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

/* Per-session led panel structure. */
struct led_panel {
	struct led leds[LEDS_COUNT];
	unsigned char leds_backup[LEDS_COUNT];
};


extern struct module leds_module;

void init_led_panel(struct led_panel *leds);

void draw_leds(struct session *ses);

struct led *register_led(struct session *ses, int number);
void unregister_led(struct led *);

#endif
