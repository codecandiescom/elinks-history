/* HTML colors parser */
/* $Id: colors.c,v 1.12 2002/12/07 20:05:54 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/colors.h"
#include "util/string.h"

struct color_spec {
	char *name;
	int rgb;
};

extern int dump_pos;

struct color_spec color_specs[] = {
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
};


#define endof(T) ((T)+sizeof(T)/sizeof(*(T)))

int
decode_color(unsigned char *str, struct rgb *col)
{
	int ch;

	if (*str != '#') {
		struct color_spec *cs;

		for (cs = color_specs; cs < endof(color_specs); cs++)
			if (!strcasecmp(cs->name, str)) {
				ch = cs->rgb;
				goto found;
			}
		str--;
	}
	str++;
	if (strlen(str) == 6) {
		char *end;

		ch = strtoul(str, &end, 16);
		if (!*end) {
found:
			col->r = ch / 0x10000;
			col->g = ch / 0x100 % 0x100;
			col->b = ch % 0x100;
			return 0;
		}
	}
	return -1;
}

/* Returns an allocated string containing name of the color or NULL if there's
 * no name for that color. */
unsigned char *
get_color_name(struct rgb *col)
{
	int color = col->r * 0x10000 + col->g * 0x100 + col->b;
	struct color_spec *cs;

	for (cs = color_specs; cs < endof(color_specs); cs++)
		if (cs->rgb == color)
			return stracpy(cs->name);

	return NULL;
}

#undef endof

/* Translate rgb color to string in #rrggbb format. str should be a pointer to
 * a 8 bytes memory space. */
void
color_to_string(struct rgb *color, unsigned char *str)
{
	snprintf(str, 8, "#%02x%02x%02x", color->r, color->g, color->b);
}


#include "document/options.h"

struct rgb palette[] = {
#if 0
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
#endif
#if 0
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
#endif
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
	{-1, -1, -1}
};

struct rgb_cache_entry {
	int color;
	int l;
	struct rgb rgb;
};


#if 0
struct rgb rgbcache = {0, 0, 0};
int rgbcache_c = 0;

static inline int find_nearest_color(struct rgb *r, int l)
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

static int
color_distance(struct rgb *c1, struct rgb *c2)
{
	return 3 * (c1->r - c2->r) * (c1->r - c2->r) +
	       4 * (c1->g - c2->g) * (c1->g - c2->g) +
	       2 * (c1->b - c2->b) * (c1->b - c2->b);
}

int
find_nearest_color(struct rgb *r, int l)
{
#define RGB_HASH_SIZE 4096
#define HASH_RGB(r, l) ((((r)->r << 3) + ((r)->g << 2) + (r)->b + (l)) & (RGB_HASH_SIZE - 1))

	int dist, dst, min, i;
	static struct rgb_cache_entry rgb_cache[RGB_HASH_SIZE];
	static int cache_init = 0;
	int h;

	if (dump_pos) {
		/* We don't ever care about colors while dumping stuff. */
		return 0;
	}

	if (!cache_init) {
		for (h = 0; h < RGB_HASH_SIZE; h++)
			rgb_cache[h].color = -1;
		cache_init = 1;
	}

	h = HASH_RGB(r, l);

	if (rgb_cache[h].color != -1 && rgb_cache[h].l == l
			&& rgb_cache[h].rgb.r == r->r && rgb_cache[h].rgb.g == r->g
			&& rgb_cache[h].rgb.b == r->b) return rgb_cache[h].color;

	dist = 0xffffff;
	min = 0;

	for (i = 0; i < l; i++) {
		dst = color_distance(r, &palette[i]);
		if (dst < dist) {
			dist = dst;
			min = i;
		}
	}

	rgb_cache[h].color = min;
	rgb_cache[h].l = l;
	rgb_cache[h].rgb.r = r->r;
	rgb_cache[h].rgb.g = r->g;
	rgb_cache[h].rgb.b = r->b;
	return min;

#undef HASH_RGB
#undef RGB_HASH_SIZE
}

int
fg_color(int fg, int bg)
{
	/* 0 == brightgrey  6 == cyan        12 == brightblue
	 * 1 == red         7 == brightgrey  13 == brightmagenta
	 * 2 == green       8 == black       14 == brightcyan
	 * 3 == red         9 == brightred   15 == brightwhite
	 * 4 == blue       10 == brightgreen
	 * 5 == magenta    11 == brightyellow
	 */

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

	return fg;
}
