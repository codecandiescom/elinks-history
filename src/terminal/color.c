/* Terminal color composing. */
/* $Id: color.c,v 1.24 2003/09/03 23:10:32 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/options.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/color.h"


static struct rgb palette[] = {
#if defined(PALA)
	{0x00, 0x00, 0x00},
	{0x80, 0x00, 0x00},
	{0x00, 0x80, 0x00},
	{0x80, 0x80, 0x00},
	{0x00, 0x00, 0x80},
	{0x80, 0x00, 0x80},
	{0x00, 0x80, 0x80},
	{0xC0, 0xC0, 0xC0},
	{0x80, 0x80, 0x80},
	{0xff, 0x00, 0x00},
	{0x00, 0xff, 0x00},
	{0xff, 0xff, 0x00},
	{0x00, 0x00, 0xff},
	{0xff, 0x00, 0xff},
	{0x00, 0xff, 0xff},
	{0xff, 0xff, 0xff},
#elif defined(PALB)
	{0x00, 0x00, 0x00},
	{0xaa, 0x00, 0x00},
	{0x00, 0xaa, 0x00},
	{0xaa, 0x55, 0x00},
	{0x00, 0x00, 0xaa},
	{0xaa, 0x00, 0xaa},
	{0x00, 0xaa, 0xaa},
	{0xaa, 0xaa, 0xaa},
	{0x55, 0x55, 0x55},
	{0xff, 0x55, 0x55},
	{0x55, 0xff, 0x55},
	{0xff, 0xff, 0x55},
	{0x55, 0x55, 0xff},
	{0xff, 0x55, 0xff},
	{0x55, 0xff, 0xff},
	{0xff, 0xff, 0xff},
#else
	{0x00, 0x00, 0x00},
	{0x80, 0x00, 0x00},
	{0x00, 0x80, 0x00},
	{0xaa, 0x55, 0x00},
	{0x00, 0x00, 0x80},
	{0x80, 0x00, 0x80},
	{0x00, 0x80, 0x80},
	{0xaa, 0xaa, 0xaa},
	{0x55, 0x55, 0x55},
	{0xff, 0x55, 0x55},
	{0x55, 0xff, 0x55},
	{0xff, 0xff, 0x55},
	{0x55, 0x55, 0xff},
	{0xff, 0x55, 0xff},
	{0x55, 0xff, 0xff},
	{0xff, 0xff, 0xff},
#endif
	{-1, -1, -1}
};

struct rgb_cache_entry {
	int color;
	int l;
	color_t rgb;
};


static inline int
color_distance(struct rgb *c1, struct rgb *c2)
{
	register int r = c1->r - c2->r;
	register int g = c1->g - c2->g;
	register int b = c1->b - c2->b;

	return (3 * r * r) + (4 * g * g) + (2 * b * b);
}

#define RED(color)	(RED_COLOR(color)   << 3)
#define GREEN(color)	(GREEN_COLOR(color) << 2)
#define BLUE(color)	(BLUE_COLOR(color)  << 0)
#define RGB(color)	(RED(color) + GREEN(color) + BLUE(color))

#define RGB_HASH_SIZE		4096
#define HASH_RGB(color, l)	((RGB(color) + (l)) & (RGB_HASH_SIZE - 1))

/* Locates the nearest terminal color. */
static inline unsigned char
find_nearest_color(color_t color, int level)
{
	static struct rgb_cache_entry rgb_fgcache[RGB_HASH_SIZE];
	struct rgb_cache_entry *rgb_cache;
	static int cache_init = 0;

	if (!cache_init) {
		register int h;

		for (h = 0; h < RGB_HASH_SIZE; h++)
			rgb_fgcache[h].color = -1;
		cache_init = 1;
	}

	rgb_cache = &rgb_fgcache[HASH_RGB(color, level)];

	if (rgb_cache->color == -1
	    || rgb_cache->l != level
	    || rgb_cache->rgb != color) {
		struct rgb rgb = INIT_RGB(color);
		unsigned char nearest_color = 0;
		int min_dist = 0xffffff;
		register int i;

		for (i = 0; i < level; i++) {
			int dist = color_distance(&rgb, &palette[i]);

			if (dist < min_dist) {
				min_dist = dist;
				nearest_color = i;
			}
		}

		rgb_cache->color = nearest_color;
		rgb_cache->l = level;
		rgb_cache->rgb = color;
	}

	return rgb_cache->color;
}

#undef HASH_RGB
#undef RGB_HASH_SIZE

/* Colors values used in the foreground color table:
 *
 *	0 == black	 8 == darkgrey (brightblack ;)
 *	1 == red	 9 == brightred
 *	2 == green	10 == brightgreen
 *	3 == brown	11 == brightyellow
 *	4 == blue	12 == brightblue
 *	5 == magenta	13 == brightmagenta
 *	6 == cyan	14 == brightcyan
 *	7 == white	15 == brightwhite
 *
 * Bright colors will be rendered bold. */

/* This table is based mostly on wild guesses of mine. Feel free to
 * correct it. --pasky */
/* Indexed by [fg][bg]->fg: */
static unsigned char fg_color[16][8] = {
	/* bk  r  gr  br  bl   m   c   w */

	/* 0 (black) */
	{  7,  0,  0,  0,  7,  0,  0,  0 },
	/* 1 (red) */
	{  1,  9,  1,  9,  9,  9,  1,  1 },
	/* 2 (green) */
	{  2,  2, 10,  2,  2,  2, 10, 10 },
	/* 3 (brown) */
	{  3, 11,  3, 11,  3, 11,  3,  3 },
	/* 4 (blue) */
	{ 12, 12,  4,  4, 12, 15,  4,  4 },
	/* 5 (magenta) */
	{  5, 13,  5, 13, 13, 13,  5,  5 },
	/* 6 (cyan) */
	{  6,  6, 14,  6,  6,  6, 14, 14 },
	/* 7 (grey) */
	{  7,  7,  0,  7,  7,  7,  0,  0 }, /* Don't s/0/8/, messy --pasky */
	/* 8 (darkgrey) */
	{ 15, 15,  8, 15, 15, 15,  8,  8 },
	/* 9 (brightred) */
	{  9,  9,  1,  9,  9,  9,  1,  9 }, /* I insist on 7->9 --pasky */
	/* 10 (brightgreen) */
	{ 10, 10, 10, 10, 10, 10, 10, 10 },
	/* 11 (brightyellow) */
	{ 11, 11, 11, 11, 11, 11, 11, 11 },
	/* 12 (brightblue) */
	{ 12, 12, 12,  4,  6,  6,  4, 12 },
	/* 13 (brightmagenta) */
	{ 13, 13,  5, 13, 13, 13,  5,  5 },
	/* 14 (brightcyan) */
	{ 14, 14, 14, 14, 14, 14, 14, 14 },
	/* 15 (brightwhite) */
	{ 15, 15, 15, 15, 15, 15, 15, 15 },
};

/* Terminal color encoding: */
/* Below color pairs are encoded to terminal colors. Both the terminal fore-
 * and background color are a number between 0 and 7. They are stored in an
 * unsigned as specified in the following bit sequence:
 *
 *	0bbb+fff (0 = not used, + = bold, f = foreground, b = background)
 */

static inline unsigned char
encode_color(struct color_pair *pair, enum screen_char_attr attr,
	     int bglevel, int fglevel)
{
	register unsigned char fg = find_nearest_color(pair->foreground, fglevel);
	register unsigned char bg = find_nearest_color(pair->background, bglevel);

	/* Adjusts the foreground color to be more visible. */
	if (d_opt && !d_opt->allow_dark_on_black) {
		fg = fg_color[fg][bg];
	}

	/* Add various color enhancement based on the attributes. */
	if (attr) {
		if (attr & SCREEN_ATTR_ITALIC)
			fg ^= 0x01;

		if (attr & SCREEN_ATTR_BOLD)
			fg |= SCREEN_ATTR_BOLD;
	}

	/* Adjusts the foreground color to be more visible. */
	if ((d_opt && !d_opt->allow_dark_on_black) || bg == fg) {
		fg = fg_color[fg][bg];
	}

	return (bg << 4 | fg);
}

unsigned char
mix_color_pair(struct color_pair *pair)
{
	return encode_color(pair, 0, 8, 16);
}

/* Defined in viewer/dump/dump.c and used to avoid calculating colors when
 * dumping stuff. */
extern int dump_pos;

unsigned char
mix_attr_colors(struct color_pair *pair, enum screen_char_attr attr,
		int bglevel, int fglevel)
{
	return (dump_pos) ? 0 : encode_color(pair, attr, bglevel, fglevel);
}
