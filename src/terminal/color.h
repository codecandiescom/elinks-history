/* $Id: color.h,v 1.5 2003/08/31 15:51:33 jonas Exp $ */

#ifndef EL__TERMINAL_COLOR_H
#define EL__TERMINAL_COLOR_H

#include "terminal/draw.h"
#include "util/color.h"

#define TERM_COLOR_BOLD	0x40
#define TERM_COLOR_MASK	0x07

#define TERM_COLOR_FOREGROUND(color) (color & TERM_COLOR_MASK)
#define TERM_COLOR_BACKGROUND(color) ((color >> 3) & TERM_COLOR_MASK)

/* Mixes the two colors to a terminal text color. */
/* If @allow_dark_on_black is non zero the foreground color will be adjusted. */
unsigned char mix_color_pair(struct color_pair *colors);

/* Mixes the two colors and adds attribute enhancements to the color. */
unsigned char mix_attr_colors(struct color_pair *colors, enum screen_char_attr attr);

#endif
