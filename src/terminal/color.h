/* $Id: color.h,v 1.25 2003/10/17 15:46:06 jonas Exp $ */

#ifndef EL__TERMINAL_COLOR_H
#define EL__TERMINAL_COLOR_H

#include "terminal/draw.h"
#include "util/color.h"

/* Terminal color encoding: */
/* Below color pairs are encoded to terminal colors. Both the terminal fore-
 * and background color are a number between 0 and 7. They are stored in an
 * unsigned char as specified in the following bit sequence:
 *
 *	0bbb0fff (f = foreground, b = background)
 */

#define TERM_COLOR_MASK	0x07

#ifdef USE_256_COLORS
#define TERM_COLOR_FOREGROUND(color) ((color)[0])
#define TERM_COLOR_BACKGROUND(color) ((color)[1])
#else
#define TERM_COLOR_FOREGROUND(color) ((color)[0] & TERM_COLOR_MASK)
#define TERM_COLOR_BACKGROUND(color) (((color)[0] >> 4) & TERM_COLOR_MASK)
#endif

/* Controls what color ranges to use when setting the terminal color. */
/* TODO: Make this rather some bitwise color_flags so we can also pass info
 *	 about allow_dark_on_black so d_opt usage is no more neede. */
enum color_type {
	COLOR_DEFAULT = 0,
	COLOR_LINK,
	COLOR_ENHANCE,

	COLOR_TYPES, /* XXX: Keep last */
};

enum color_mode {
	COLOR_MODE_MONO = 0,
	COLOR_MODE_16,
	COLOR_MODE_256,
	COLOR_MODE_DUMP,

	COLOR_MODES, /* XXX: Keep last */
};

/* Mixes the color pair and attributes to a terminal text color. */
/* If @allow_dark_on_black is non zero the foreground color will be adjusted.
 * XXX: @schar may not be NULL and is modified adding stuff like boldness. */
void set_term_color(struct screen_char *schar, struct color_pair *pair,
		    enum color_type type, enum color_mode mode);

#endif
