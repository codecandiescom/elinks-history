/* $Id: color.h,v 1.3 2003/08/29 11:16:58 jonas Exp $ */

#ifndef EL__TERMINAL_COLOR_H
#define EL__TERMINAL_COLOR_H

#include "terminal/draw.h"
#include "util/color.h"

/* Mixes the two colors to a terminal text color. */
/* If @allow_dark_on_black is non zero the foreground color will be adjusted. */
unsigned char mix_color_pair(struct color_pair *colors);

/* Mixes the two colors and adds attribute enhancements to the color. */
unsigned char mix_attr_colors(struct color_pair *colors, enum screen_char_attr attr);

#endif
