/* HTML colors parser */
/* $Id: colors.c,v 1.25 2003/07/31 17:29:00 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/colors.h"
#include "document/options.h"
#include "util/fastfind.h"
#include "util/string.h"

struct color_spec {
	char *name;
	int rgb;
};

extern int dump_pos;

static struct color_spec color_specs[] = {
	{"aliceblue",		0xF0F8FF},
	{"antiquewhite",	0xFAEBD7},
	{"aqua",		0x00FFFF},
	{"aquamarine",		0x7FFFD4},
	{"azure",		0xF0FFFF},
	{"beige",		0xF5F5DC},
	{"bisque",		0xFFE4C4},
	{"black",		0x000000},
	{"blanchedalmond",	0xFFEBCD},
	{"blue",		0x0000FF},
	{"blueviolet",		0x8A2BE2},
	{"brown",		0xA52A2A},
	{"burlywood",		0xDEB887},
	{"cadetblue",		0x5F9EA0},
	{"chartreuse",		0x7FFF00},
	{"chocolate",		0xD2691E},
	{"coral",		0xFF7F50},
	{"cornflowerblue",	0x6495ED},
	{"cornsilk",		0xFFF8DC},
	{"crimson",		0xDC143C},
	{"cyan",		0x00FFFF},
	{"darkblue",		0x00008B},
	{"darkcyan",		0x008B8B},
	{"darkgoldenrod",	0xB8860B},
	{"darkgray",		0xA9A9A9},
	{"darkgreen",		0x006400},
	{"darkkhaki",		0xBDB76B},
	{"darkmagenta",		0x8B008B},
	{"darkolivegreen",	0x556B2F},
	{"darkorange",		0xFF8C00},
	{"darkorchid",		0x9932CC},
	{"darkred",		0x8B0000},
	{"darksalmon",		0xE9967A},
	{"darkseagreen",	0x8FBC8F},
	{"darkslateblue",	0x483D8B},
	{"darkslategray",	0x2F4F4F},
	{"darkturquoise",	0x00CED1},
	{"darkviolet",		0x9400D3},
	{"deeppink",		0xFF1493},
	{"deepskyblue",		0x00BFFF},
	{"dimgray",		0x696969},
	{"dodgerblue",		0x1E90FF},
	{"firebrick",		0xB22222},
	{"floralwhite",		0xFFFAF0},
	{"forestgreen",		0x228B22},
	{"fuchsia",		0xFF00FF},
	{"gainsboro",		0xDCDCDC},
	{"ghostwhite",		0xF8F8FF},
	{"gold",		0xFFD700},
	{"goldenrod",		0xDAA520},
	{"gray",		0x808080},
	{"green",		0x008000},
	{"greenyellow",		0xADFF2F},
	{"honeydew",		0xF0FFF0},
	{"hotpink",		0xFF69B4},
	{"indianred",		0xCD5C5C},
	{"indigo",		0x4B0082},
	{"ivory",		0xFFFFF0},
	{"khaki",		0xF0E68C},
	{"lavender",		0xE6E6FA},
	{"lavenderblush",	0xFFF0F5},
	{"lawngreen",		0x7CFC00},
	{"lemonchiffon",	0xFFFACD},
	{"lightblue",		0xADD8E6},
	{"lightcoral",		0xF08080},
	{"lightcyan",		0xE0FFFF},
	{"lightgoldenrodyellow",	0xFAFAD2},
	{"lightgreen",		0x90EE90},
	{"lightgrey",		0xD3D3D3},
	{"lightpink",		0xFFB6C1},
	{"lightsalmon",		0xFFA07A},
	{"lightseagreen",	0x20B2AA},
	{"lightskyblue",	0x87CEFA},
	{"lightslategray",	0x778899},
	{"lightsteelblue",	0xB0C4DE},
	{"lightyellow",		0xFFFFE0},
	{"lime",		0x00FF00},
	{"limegreen",		0x32CD32},
	{"linen",		0xFAF0E6},
	{"magenta",		0xFF00FF},
	{"maroon",		0x800000},
	{"mediumaquamarine",	0x66CDAA},
	{"mediumblue",		0x0000CD},
	{"mediumorchid",	0xBA55D3},
	{"mediumpurple",	0x9370DB},
	{"mediumseagreen",	0x3CB371},
	{"mediumslateblue",	0x7B68EE},
	{"mediumspringgreen",	0x00FA9A},
	{"mediumturquoise",	0x48D1CC},
	{"mediumvioletred",	0xC71585},
	{"midnightblue",	0x191970},
	{"mintcream",		0xF5FFFA},
	{"mistyrose",		0xFFE4E1},
	{"moccasin",		0xFFE4B5},
	{"navajowhite",		0xFFDEAD},
	{"navy",		0x000080},
	{"oldlace",		0xFDF5E6},
	{"olive",		0x808000},
	{"olivedrab",		0x6B8E23},
	{"orange",		0xFFA500},
	{"orangered",		0xFF4500},
	{"orchid",		0xDA70D6},
	{"palegoldenrod",	0xEEE8AA},
	{"palegreen",		0x98FB98},
	{"paleturquoise",	0xAFEEEE},
	{"palevioletred",	0xDB7093},
	{"papayawhip",		0xFFEFD5},
	{"peachpuff",		0xFFDAB9},
	{"peru",		0xCD853F},
	{"pink",		0xFFC0CB},
	{"plum",		0xDDA0DD},
	{"powderblue",		0xB0E0E6},
	{"purple",		0x800080},
	{"red",			0xFF0000},
	{"rosybrown",		0xBC8F8F},
	{"royalblue",		0x4169E1},
	{"saddlebrown",		0x8B4513},
	{"salmon",		0xFA8072},
	{"sandybrown",		0xF4A460},
	{"seagreen",		0x2E8B57},
	{"seashell",		0xFFF5EE},
	{"sienna",		0xA0522D},
	{"silver",		0xC0C0C0},
	{"skyblue",		0x87CEEB},
	{"slateblue",		0x6A5ACD},
	{"slategray",		0x708090},
	{"snow",		0xFFFAFA},
	{"springgreen",		0x00FF7F},
	{"steelblue",		0x4682B4},
	{"tan",			0xD2B48C},
	{"teal",		0x008080},
	{"thistle",		0xD8BFD8},
	{"tomato",		0xFF6347},
	{"turquoise",		0x40E0D0},
	{"violet",		0xEE82EE},
	{"wheat",		0xF5DEB3},
	{"white",		0xFFFFFF},
	{"whitesmoke",		0xF5F5F5},
	{"yellow",		0xFFFF00},
	{"yellowgreen",		0x9ACD32},
	{NULL,			0}
};

#ifdef USE_FASTFIND

static struct fastfind_info *ff_info_colors;
static struct color_spec *internal_pointer;

/* Reset internal list pointer */
void
colors_list_reset(void)
{
	internal_pointer = color_specs;
}

/* Returns a pointer to a struct that contains
 * current key and data pointers and increment
 * internal pointer.
 * It returns NULL when key is NULL. */
struct fastfind_key_value *
colors_list_next(void)
{
	static struct fastfind_key_value kv;

	if (!internal_pointer->name) return NULL;

	kv.key = (unsigned char *) internal_pointer->name;
	kv.data = internal_pointer;

	internal_pointer++;

	return &kv;
}

#endif /* USE_FASTFIND */

void
init_colors_lookup(void)
{
#ifdef USE_FASTFIND
	ff_info_colors = fastfind_index(&colors_list_reset,
					&colors_list_next,
					0);
	fastfind_index_compress(ff_info_colors);
#endif
}

void
free_colors_lookup(void)
{
#ifdef USE_FASTFIND
	fastfind_done(ff_info_colors);
#endif
}

#define int2rgb(n, col) (col)->r = (n) / 0x10000,	\
			(col)->g = (n) / 0x100 % 0x100,	\
			(col)->b = (n) % 0x100

int
decode_color(unsigned char *str, struct rgb *col)
{
	int slen = strlen(str);

	if (*str == '#' && slen == 7) {
		unsigned char *end;
		int ch;

		errno = 0;
		ch = strtoul(&str[1], (char **)&end, 16);
		if (!errno && !*end) {
			int2rgb(ch, col);
			return 0;
		}
	} else {
#ifndef USE_FASTFIND
		register struct color_spec *cs = color_specs;

		while (cs->name) {
			if (!strcasecmp(cs->name, str)) {
				int2rgb(cs->rgb, col);
				return 0;
			}
			cs++;
		}
#else
		struct color_spec *cs;

		cs = (struct color_spec *) fastfind_search(str, slen, ff_info_colors);

		if (cs) {
			int2rgb(cs->rgb, col);
			return 0;
		}
#endif
	}

	return -1; /* Not found */
}

#undef int2rgb

/* Returns an allocated string containing name of the color or NULL if there's
 * no name for that color. */
unsigned char *
get_color_name(struct rgb *col)
{
	int color = col->r * 0x10000 + col->g * 0x100 + col->b;
	register struct color_spec *cs = color_specs;

	while (cs->name) {
		if (cs->rgb == color)
			return stracpy(cs->name);
		cs++;
	}

	return NULL;
}

/* Translate rgb color to string in #rrggbb format. str should be a pointer to
 * a 8 bytes memory space. */
void
color_to_string(struct rgb *color, unsigned char *str)
{
	snprintf(str, 8, "#%02x%02x%02x", color->r, color->g, color->b);
}

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
	struct rgb rgb;
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
	return 3 * (c1->r - c2->r) * (c1->r - c2->r) +
	       4 * (c1->g - c2->g) * (c1->g - c2->g) +
	       2 * (c1->b - c2->b) * (c1->b - c2->b);
}

unsigned char
find_nearest_color(struct rgb *rgb, int l)
{
#define RGB_HASH_SIZE 4096
#define HASH_RGB(rgb, l) ((((rgb)->r << 3) + ((rgb)->g << 2) + (rgb)->b + (l)) & (RGB_HASH_SIZE - 1))

	int min_dist, dist, i;
	static struct rgb_cache_entry rgb_fgcache[RGB_HASH_SIZE];
	/*static struct rgb_cache_entry rgb_bgcache[RGB_HASH_SIZE];*/
	struct rgb_cache_entry *rgb_cache = /*l == 8 ? rgb_bgcache :*/ rgb_fgcache;
	static int cache_init = 0;
	unsigned char color;
	int h;

	/* We don't ever care about colors while dumping stuff. */
	if (dump_pos) return 0;

	if (!cache_init) {
		for (h = 0; h < RGB_HASH_SIZE; h++)
			rgb_cache[h].color = -1;
		cache_init = 1;
	}

	h = HASH_RGB(rgb, l);

	if (rgb_cache[h].color != -1
	    && rgb_cache[h].l == l
	    && rgb_cache[h].rgb.r == rgb->r
	    && rgb_cache[h].rgb.g == rgb->g
	    && rgb_cache[h].rgb.b == rgb->b)
		return rgb_cache[h].color;

	min_dist = 0xffffff;
	color = 0;

	for (i = 0; i < l; i++) {
		dist = color_distance(rgb, /*l==8 ? &bgpalette[i] :*/ &palette[i]);
		if (dist < min_dist) {
			min_dist = dist;
			color = i;
		}
	}

	rgb_cache[h].color = color;
	rgb_cache[h].l = l;
	rgb_cache[h].rgb.r = rgb->r;
	rgb_cache[h].rgb.g = rgb->g;
	rgb_cache[h].rgb.b = rgb->b;

	return color;

#undef HASH_RGB
#undef RGB_HASH_SIZE
}

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
	int xlat[16][8] = {
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
		{ 12, 12,  4,  4, 12,  4,  4,  4 },
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

	if (!d_opt->allow_dark_on_black)
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
