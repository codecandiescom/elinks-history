/* $Id: color.h,v 1.1 2003/08/23 16:33:22 jonas Exp $ */

#ifndef EL__UTIL_COLOR_H
#define EL__UTIL_COLOR_H

typedef unsigned long color_t;

struct rgb {
	unsigned char r, g, b;
	unsigned char pad;
};

#define INT2RGB(color, rgb) \
	do { \
		(rgb).r = ((color) >> 16) & 0xFF; \
		(rgb).g = ((color) >>  8) & 0xFF; \
		(rgb).b = ((color) >>  0) & 0xFF; \
	} while (0)

struct color_pair {
	color_t background;
	color_t foreground;
};

#define INIT_COLOR_PAIR(bg, fg) { bg, fg }

/* Decode the color string. */
/* The color string can either contain '#FF0044' style declarations or
 * color names. */
int decode_color(unsigned char *str, color_t *color);

/* Returns an allocated string containing name of the @color or NULL if there's
 * no name for that color. */
unsigned char *get_color_name(color_t color);

/* Translate rgb color to string in #rrggbb format. str should be a pointer to
 * a 8 bytes memory space. */
void color_to_string(color_t color, unsigned char str[]);

/* Fastfind lookup management. */
void init_colors_lookup(void);
void free_colors_lookup(void);

#endif
