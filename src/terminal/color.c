/* Terminal color composing. */
/* $Id: color.c,v 1.10 2003/08/30 20:51:08 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/options.h"
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

#define RGB_HASH_SIZE 4096
#define HASH_RGB(rgb, l) ((((rgb).r << 3) + \
			   ((rgb).g << 2) + \
			    (rgb).b + (l)) & (RGB_HASH_SIZE - 1))

/* Locates the nearest terminal color. */
/* Hint: @level should be 16 for foreground colors and 8 for backgrounds. */
static inline unsigned char
find_nearest_color(color_t color, int level)
{
	static struct rgb_cache_entry rgb_fgcache[RGB_HASH_SIZE];
	struct rgb_cache_entry *rgb_cache = rgb_fgcache;
	struct rgb rgb;
	static int cache_init = 0;
	register int h, i;
	int min_dist = 0xffffff;
	unsigned char nearest_color = 0;

	if (!cache_init) {
		for (h = 0; h < RGB_HASH_SIZE; h++)
			rgb_cache[h].color = -1;
		cache_init = 1;
	}

	INT2RGB(color, rgb);
	h = HASH_RGB(rgb, level);

	if (rgb_cache[h].color != -1
	    && rgb_cache[h].l == level
	    && rgb_cache[h].rgb == color)
		return rgb_cache[h].color;

	for (i = 0; i < level; i++) {
		int dist = color_distance(&rgb, /*l==8 ? &bgpalette[i] :*/ &palette[i]);

		if (dist < min_dist) {
			min_dist = dist;
			nearest_color = i;
		}
	}

	rgb_cache[h].color = nearest_color;
	rgb_cache[h].l = level;
	rgb_cache[h].rgb = color;

	return nearest_color;
}

#undef HASH_RGB
#undef RGB_HASH_SIZE

/* Adjusts the foreground color to be more visible on the background. */
static inline unsigned char
fg_color(unsigned char fg, unsigned char bg)
{
	/* 0 == black       6 == cyan        12 == brightblue
	 * 1 == red         7 == brightgrey  13 == brightmagenta
	 * 2 == green       8 == darkgrey    14 == brightcyan
	 * 3 == brown       9 == brightred   15 == brightwhite
	 * 4 == blue       10 == brightgreen
	 * 5 == magenta    11 == brightyellow
	 */

	/* This table is based mostly on wild guesses of mine. Feel free to
	 * correct it. --pasky */
	/* Indexed by [fg][bg]->fg: */
	static int xlat[16][8] = {
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
		{ 15, 15, 15, 15, 15, 15, 15,  7 },
	};

	if (d_opt && !d_opt->allow_dark_on_black)
		return xlat[fg][bg];
	else
		return fg;
}

/* TODO: Either only #define mix_color_pair() in header file to use
 * mix_attr_colors() as backend or reduce code duplication some other way. */

/* Terminal color encoding: */
/* Below color pairs are encoded to terminal colors. Both the terminal fore-
 * and background color are a number between 0 and 7. They are stored in an
 * unsigned as specified in the following bit sequence:
 *
 *	00bbbfff (0 = not used, f = foreground bit, b = background bit)
 */

#if YOU_WANT_TO_TRY_SOMETHING_WEIRD
#define encode_colors(color, bg, fg) \
	do { \
		color = ((fg & 0x08) << 3) | (bg << 3) | (fg & 0x07); \
		\
		if (!(color & 0x40) && bg == fg && !(bg & 0x02)) \
			color |= 0x07; \
	} while (0)
#else
#define encode_color(color, bg, fg) \
	do { \
		color = ((fg & 0x08) << 3) | (bg << 3) | (fg & 0x07); \
		\
		if (!(color & 0x40) && bg == (fg & 0x07)) \
			color = (color & 0x38) | 7 * !(color & 0x10); \
	} while (0)
#endif
			

unsigned char
mix_color_pair(struct color_pair *pair)
{
	register unsigned char fg = find_nearest_color(pair->foreground, 16);
	register unsigned char bg = find_nearest_color(pair->background, 8);
	register unsigned char color;

	fg = fg_color(fg, bg);

	encode_color(color, bg, fg);

	return color;
}

/* Defined in viewer/dump/dump.c */
extern int dump_pos;

unsigned char
mix_attr_colors(struct color_pair *pair, enum screen_char_attr attr)
{
	register unsigned char fg;
	register unsigned char bg;
	register unsigned char color;

	/* We don't ever care about colors while dumping stuff. */
	if (dump_pos) return 0;

	fg = find_nearest_color(pair->foreground, 16);
	bg = find_nearest_color(pair->background, 8);

	if (attr) {
		if (attr & SCREEN_ATTR_ITALIC)
			fg ^= 0x01;

		if (attr & SCREEN_ATTR_UNDERLINE)
			fg = (fg ^ 0x04) | 0x08;

		if (attr & SCREEN_ATTR_BOLD)
			fg |= 0x08;

#if 0
		/* AT_GRAPHICS is currently only used for <hr /> tags.
		 * Dunno if this (old code) makes sense? --jonas */
		if (attr & AT_GRAPHICS) bg = bg | 0x10;
#endif
	}

	fg = fg_color(fg, bg);

	encode_color(color, bg, fg);

	return color;
}
