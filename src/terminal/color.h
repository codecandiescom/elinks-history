/* $Id: color.h,v 1.14 2003/09/08 15:22:45 jonas Exp $ */

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

#define TERM_COLOR_FOREGROUND(color) (color & TERM_COLOR_MASK)
#define TERM_COLOR_BACKGROUND(color) ((color >> 4) & TERM_COLOR_MASK)

/* Mixes the color pair and attributes to a terminal text color. */
/* If @allow_dark_on_black is non zero the foreground color will be adjusted.
 * XXX: @attr may not be NULL and is modified adding stuff like boldness. */
void set_term_color8(struct screen_char *schar, struct color_pair *pair,
		     int bglevel, int fglevel);

#endif
