/* Terminal color composing. */
/* $Id: color.c,v 1.3 2003/08/26 23:52:45 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/options.h"
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

#if 0
static struct rgb bgpalette[] = {
	{0x22, 0x22, 0x22},
	{0xbb, 0x22, 0x22},
	{0x22, 0xbb, 0x22},
	{0xcc, 0xbb, 0x22},
	{0x22, 0x22, 0xbb},
	{0xbb, 0x22, 0xbb},
	{0x22, 0xbb, 0xbb},
	{0xcc, 0xcc, 0xcc},
	{-1, -1, -1}
};
#endif

struct rgb_cache_entry {
	int color;
	int l;
	color_t rgb;
};


#if 0
struct rgb rgbcache = {0, 0, 0};
int rgbcache_c = 0;

static inline int
find_nearest_color(struct rgb *r, int l)
{
	int dist, dst, min, i;
	if (r->r == rgbcache.r && r->g == rgbcache.g && r->b == rgbcache.b) return rgbcache_c;
	dist = 0xffffff;
	min = 0;
	for (i = 0; i < l; i++) if ((dst = color_distance(r, &palette[i])) < dist)
		dist = dst, min = i;
	return min;
}
#endif

static inline int
color_distance(struct rgb *c1, struct rgb *c2)
{
	register int r = c1->r - c2->r;
	register int g = c1->g - c2->g;
	register int b = c1->b - c2->b;

	return 3 * r * r +
	       4 * g * g +
	       2 * b * b;
}

/* Defined in viewer/dump/dump.c */
extern int dump_pos;

#define RGB_HASH_SIZE 4096
#define HASH_RGB(rgb, l) ((((rgb).r << 3) + \
			   ((rgb).g << 2) + \
			    (rgb).b + (l)) & (RGB_HASH_SIZE - 1))

unsigned char
find_nearest_color(color_t color, int l)
{
	static struct rgb_cache_entry rgb_fgcache[RGB_HASH_SIZE];
	/*static struct rgb_cache_entry rgb_bgcache[RGB_HASH_SIZE];*/
	struct rgb_cache_entry *rgb_cache = /*l == 8 ? rgb_bgcache :*/ rgb_fgcache;
	struct rgb rgb;
	static int cache_init = 0;
	register int h, i;
	int min_dist = 0xffffff;
	unsigned char nearest_color = 0;

	/* We don't ever care about colors while dumping stuff. */
	if (dump_pos) return 0;

	if (!cache_init) {
		for (h = 0; h < RGB_HASH_SIZE; h++)
			rgb_cache[h].color = -1;
		cache_init = 1;
	}

	INT2RGB(color, rgb);
	h = HASH_RGB(rgb, l);

	if (rgb_cache[h].color != -1
	    && rgb_cache[h].l == l
	    && rgb_cache[h].rgb == color)
		return rgb_cache[h].color;

	for (i = 0; i < l; i++) {
		int dist = color_distance(&rgb, /*l==8 ? &bgpalette[i] :*/ &palette[i]);

		if (dist < min_dist) {
			min_dist = dist;
			nearest_color = i;
		}
	}

	rgb_cache[h].color = nearest_color;
	rgb_cache[h].l = l;
	rgb_cache[h].rgb = color;

	return nearest_color;
}
#undef HASH_RGB
#undef RGB_HASH_SIZE

int
fg_color(int fg, int bg)
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

	/* This is the original code - you can't really guess from it what is
	 * translated to what and easily modify it. Also, it supports well only
	 * the black background. */
#if 0
	/* This looks like it should be more efficient. It results in
	 * different machine-code, but the same number of instructions:
	 * int l, h;
	 * if (bg < fg) l = bg, h = fg; else l = fg, h = bg;
	 */
	int l = bg < fg ? bg : fg;
	int h = bg < fg ? fg : bg;

	if (l == h
		/* Check for clashing colours. For example, 3 (red) clashes
		 * with 5 (magenta) and 12 (brightblue). */
		|| (l == 0 && (h == 8))
		|| (l == 1 && (h == 3 || h == 5 || h == 12))
		|| (l == 2 && (h == 6))
		|| (l == 3 && (h == 5 || h == 12))
		|| ((l == 4 || l == 5) && (h == 8 || h == 12))
		|| (!d_opt->allow_dark_on_black &&
			/* ^- FIXME: when possibility to change bg color... */
			   ((l == 0 && (h == 4 || h == 12)) ||
			    (l == 1 && (h == 8)))
		   )
	   )
		return (fg == 4 || fg == 12)
			&& (bg == 0 || bg == 8) ? 6
						: (7 - 7 * (bg == 2 ||
							    bg == 6 ||
							    bg == 7));
#endif
	return fg;
}

unsigned char
mix_color_pair(struct color_pair *color)
{
	register unsigned char fg = find_nearest_color(color->foreground, 16);
	register unsigned char bg = find_nearest_color(color->background, 8);

	fg = fg_color(fg, bg);

	return ((fg & 0x08) << 3) | (bg << 3) | (fg & 0x07);
}

#if 0
unsigned char
mix_attr_colors(color_t background, color_t foreground,
		enum screen_char_attr attr, enum term_mode_type type)
{
	register unsigned char fg = find_nearest_color(foreground, 16);
	register unsigned char bg = find_nearest_color(background, 8);

	if (attr) {
		if (attr & SCREEN_ATTR_ITALIC)
			fg ^= 0x01;

		/* If the term don't support underline flip som color bits. */
		if ((attr & SCREEN_ATTR_UNDERLINE) && type != TERM_VT100)
			fg = (fg ^ 0x04) | 0x08;

		if (attr & SCREEN_ATTR_BOLD)
			fg |= 0x08;

#if 0
		/* AT_GRAPHICS is currently only used for <hr /> tags so dunno
		 * if this (old code) makes sense?
		 * Else add a new SCREEN_ATTR_* flag. --jonas */
		if (attr & AT_GRAPHICS) bg = bg | 0x10;
#endif

	}

	fg = fg_color(fg, bg);

	return ((fg & 0x08) << 3) | (bg << 3) | (fg & 0x07);
}
#endif
