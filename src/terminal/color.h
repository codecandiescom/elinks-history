/* $Id: color.h,v 1.12 2003/09/06 15:57:33 jonas Exp $ */

#ifndef EL__TERMINAL_COLOR_H
#define EL__TERMINAL_COLOR_H

#include "terminal/draw.h"
#include "util/color.h"

#define TERM_COLOR_MASK	0x07

#define TERM_COLOR_FOREGROUND(color) (color & TERM_COLOR_MASK)
#define TERM_COLOR_BACKGROUND(color) ((color >> 4) & TERM_COLOR_MASK)

/* Mixes the color pair and attributes to a terminal text color. */
/* If @allow_dark_on_black is non zero the foreground color will be adjusted.
 * XXX: @attr may not be NULL and is modified adding stuff like boldness. */
unsigned char get_term_color8(struct color_pair *pair, int bglevel, int fglevel,
			      enum screen_char_attr *attr);

#endif
