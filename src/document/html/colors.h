/* $Id: colors.h,v 1.9 2003/08/23 04:44:58 jonas Exp $ */

#ifndef EL__DOCUMENT_HTML_COLORS_H
#define EL__DOCUMENT_HTML_COLORS_H

typedef unsigned long color_t;

#define set_rgb_color(color, red, green, blue) \
	do { \
		(red)	= ((color) >> 16) & 0xFF; \
		(green)	= ((color) >>  8) & 0xFF; \
		(blue)	= ((color) >>  0) & 0xFF; \
	} while (0)

int decode_color(unsigned char *, color_t *);
unsigned char * get_color_name(color_t);
void color_to_string(color_t, unsigned char *);

unsigned char find_nearest_color(color_t, int);
int fg_color(int, int);

void init_colors_lookup(void);
void free_colors_lookup(void);

#endif
