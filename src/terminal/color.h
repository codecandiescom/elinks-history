/* $Id: color.h,v 1.2 2003/08/24 02:53:27 jonas Exp $ */

#ifndef EL__TERMINAL_COLOR_H
#define EL__TERMINAL_COLOR_H

#include "util/color.h"

/* Mixes the two colors to a terminal text color. */
/* If @allow_dark_on_black is non zero the foreground color will be adjusted. */
unsigned char mix_color_pair(struct color_pair *colors);

#if 0
#include "terminal/draw.h"
#include "terminal/terminal.h"

/* Mixes the two colors and adds attribute enhancements to the color. */
unsigned char mix_attr_colors(color_t background, color_t foreground,
			      enum screen_char_attr, enum term_mode_type);
#endif

/* Locates the nearest terminal color. */
/* Hint: @level should be 16 for foreground colors and 8 for backgrounds. */
unsigned char find_nearest_color(color_t color, int level);

/* Adjusts the foreground color to be more visible on the background. */
int fg_color(int fg, int bg);

#endif
