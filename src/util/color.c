/* Color parser */
/* $Id: color.c,v 1.18 2004/06/25 10:52:31 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "util/color.h"
#include "util/fastfind.h"
#include "util/string.h"

struct color_spec {
	char *name;
	color_t rgb;
};

static struct color_spec color_specs[] = {
#include "util/color_s.inc"
#ifndef CONFIG_SMALL
#include "util/color.inc"
#endif
	{ NULL,	0}
};

#ifdef USE_FASTFIND

static struct fastfind_info *ff_info_colors;
static struct color_spec *internal_pointer;

static void
colors_list_reset(void)
{
	internal_pointer = color_specs;
}

/* Returns a pointer to a struct that contains
 * current key and data pointers and increment
 * internal pointer.
 * It returns NULL when key is NULL. */
static struct fastfind_key_value *
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
					0, "colors_lookup");
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

int
decode_color(unsigned char *str, int slen, color_t *color)
{
	if (*str == '#' && (slen == 7 || slen == 4)) {
		unsigned char buffer[7];
		unsigned char *end;
		color_t string_color;

		str++;

		if (slen == 4) {
			/* Expand the short hex color format */
			buffer[0] = buffer[1] = str[0];
			buffer[2] = buffer[3] = str[1];
			buffer[4] = buffer[5] = str[2];
			buffer[6] = 0;
			str = buffer;
		}

		errno = 0;
		string_color = strtoul(str, (char **) &end, 16);
		if (!errno && (end == str + 6)) {
			*color = string_color;
			return 0;
		}
	} else {
		struct color_spec *cs;

#ifndef USE_FASTFIND
		for (cs = color_specs; cs->name; cs++)
			if (!strlcasecmp(cs->name, -1, str, slen))
				break;
#else
		cs = fastfind_search(str, slen, ff_info_colors);
#endif
		if (cs && cs->name) {
			*color = cs->rgb;
			return 0;
		}
	}

	return -1; /* Not found */
}

unsigned char *
get_color_string(color_t color, unsigned char hexcolor[8])
{
	struct color_spec *cs;

	for (cs = color_specs; cs->name; cs++)
		if (cs->rgb == color)
			return cs->name;

	color_to_string(color, hexcolor);
	return hexcolor;
}

void
color_to_string(color_t color, unsigned char str[])
{
	snprintf(str, 8, "#%06lx", (unsigned long) color);
}
