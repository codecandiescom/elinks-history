/* Terminal color composing. */
/* $Id: color.c,v 1.31 2003/09/06 20:33:38 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/options.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "util/color.h"


/* Thanks to deltab for this precious document! This is the only reference
 * found by google regarding passing RGB color triplets to xterm. It was found
 * indirectly when looking for details regarding xterm256, it was the only
 * other match for our query and it comes from some postscript on a Russian
 * server. Deltab was reportedly looking for this for years ;-).
 *
 * I T U - T T.416 TELECOMMUNICATION (03/93) STANDARDIZATION SECTOR OF ITU
 * TELEMATIC SERVICES TERMINAL EQUIPMENTS AND PROTOCOLS FOR TELEMATIC SERVICES
 * CHARACTER CONTENT ARCHITECTURES ITU-T Recommendation T.416 (Previously "CCITT Recommendation")
 *
 * ...
 *
 * 40 ITU-T Rec. T.416 (1993 E)
 *
 * ISO/IEC 8613-6 : 1994 (E) These values are included in this Specification only
 * for compatibility with some existing applications such as those based upon
 * Recommendation T.61 (1984).
 *
 * ...
 *
 * The parameter values 38 and 48 are followed by a parameter substring used to
 * select either the character foreground "colour value" or the character
 * background "colour value".
 *
 * A parameter substring for values 38 or 48 may be divided by one or more
 * separators (03/10) into parameter elements, denoted as Pe. The format of such a
 * parameter sub-string is indicated as:
 *
 * Pe : P ... Each parameter element consists of zero, one or more bit
 * combinations from 03/00 to 03/09, representing the digits 0 to 9. An empty
 * parameter element represents a default value for this parameter element. Empty
 * parameter elements at the end of the parameter substring need not be included.
 * The first parameter element indicates a choice between:
 *
 * 0 implementation defined (only applicable for the character foreground colour)
 * 1 transparent; 2 direct colour in RGB space; 3 direct colour in CMY space; 4
 * direct colour in CMYK space; 5 indexed colour.
 *
 * If the first parameter has the value 0 or 1, there are no additional parameter
 * elements. If the first parameter element has the value 5, then there is a
 * second parameter element specifying the index into the colourtable given by the
 * attribute "content colour table" applying to the object with which the content
 * is associated.
 *
 * If the first parameter element has the value 2, 3, or 4, the second parameter
 * element specifies a colour space identifier referring to a colour space
 * definition in the document profile. If the first parameter element has the
 * value 2, the parameter elements 3, 4, and 5, are three integers for red, green,
 * and blue colour components. Parameter 6 has no meaning. If the first parameter
 * has the value 3, the parameter elements 3, 4, and 5 and three integers for
 * cyan, magenta, and yellowcolour components. Parameter 6 has no meaning. If the
 * first parameter has the value 4, the parameter elements 3, 4, 5, and 6, are
 * four integers for cyan, magenta, yellow, and black colour components. If the
 * first parameter element has the value 2, 3, or 4, the parameter element 7 may
 * be used to specify a tolerance value (an integer) and the parameter element 8
 * first parameter element has the value 2, 3, or 4, the parameter element 7 may
 * be used to specify a tolerance value (an integer) and the parameter element 8
 * may be used to specify a colour space associated with the tolerance (0 for
 * CIELUV, 1 for CIELAB).
 *
 * NOTE 3 - The "colour space id" component will refer to the applicable colour
 * space description in the document profile which may contain colour scaling data
 * that describe the scale and offset to be applied to the specified colour
 * components in the character content. Appropriate use of scaling and offsets may
 * be required to map all colour values required into the integer encodingspace
 * provided. This may be particularly important if concatenated content requires
 * the insertion of such SGR sequences by the content layout process. */


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
	int level;
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

#if 0
	/* No need to poison the cache since calling this function is only
	 * meaningfull when level > 0 */
	static int cache_init = 0;

	if (!cache_init) {
		register int h;

		for (h = 0; h < RGB_HASH_SIZE; h++)
			rgb_fgcache[h].color = -1;
		cache_init = 1;
	}
#endif

	rgb_cache = &rgb_fgcache[HASH_RGB(color, level)];

	if (rgb_cache->level == 0
	    || rgb_cache->level != level
	    || rgb_cache->rgb != color) {
		struct rgb rgb = INIT_RGB(color);
		unsigned char nearest_color = 0;
		int min_dist = 0xffffff;
		register int i;

		/* This is a hotspot so maybe this is a bad idea. --jonas */
		assertm(level, "find_nearest_color() called with @level = 0");

		for (i = 0; i < level; i++) {
			int dist = color_distance(&rgb, &palette[i]);

			if (dist < min_dist) {
				min_dist = dist;
				nearest_color = i;
			}
		}

		rgb_cache->color = nearest_color;
		rgb_cache->level = level;
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

/* Defined in viewer/dump/dump.c and used to avoid calculating colors when
 * dumping stuff. */
extern int dump_pos;

/* When determining wether to use negative image we make the most significant
 * be least significant. */
#define CMPCODE(c) (((c) << 1 | (c) >> 2) & TERM_COLOR_MASK)
#define use_inverse(bg, fg) CMPCODE(fg & TERM_COLOR_MASK) < CMPCODE(bg)

/* Terminal color encoding: */
/* Below color pairs are encoded to terminal colors. Both the terminal fore-
 * and background color are a number between 0 and 7. They are stored in an
 * unsigned as specified in the following bit sequence:
 *
 *	00bbbfff (f = foreground, b = background)
 */

unsigned char
get_term_color8(struct color_pair *pair, int bglevel, int fglevel,
		enum screen_char_attr *attr)
{
	register unsigned char fg;
	register unsigned char bg;

	assert(attr);

	if (dump_pos) return 0;

	fg = find_nearest_color(pair->foreground, fglevel);
	bg = find_nearest_color(pair->background, bglevel);

	/* Adjusts the foreground color to be more visible. */
	if (d_opt && !d_opt->allow_dark_on_black) {
		fg = fg_color[fg][bg];
	}

	/* Add various color enhancement based on the attributes. */
	if (*attr) {
		if (*attr & SCREEN_ATTR_ITALIC)
			fg ^= 0x01;

		if (*attr & SCREEN_ATTR_BOLD)
			fg |= SCREEN_ATTR_BOLD;
	}

	/* Adjusts the foreground color to be more visible. */
	if ((d_opt && !d_opt->allow_dark_on_black) || bg == fg) {
		fg = fg_color[fg][bg];

		if (fg & SCREEN_ATTR_BOLD) {
			*attr |= SCREEN_ATTR_BOLD;
		}
	}

	if (use_inverse(bg, fg)) {
		*attr |= SCREEN_ATTR_STANDOUT;
	}

	return (bg << 4 | fg);
}
