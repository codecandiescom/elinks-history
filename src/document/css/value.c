/* CSS property value parser */
/* $Id: value.c,v 1.12 2004/01/17 23:05:53 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/property.h"
#include "document/css/value.h"
#include "document/html/parser.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


typedef int (*css_value_parser_t)(union css_decl_value *value,
				  unsigned char **string);


static int
rgb_component_parser(unsigned char **string, unsigned char terminator)
{
	unsigned char *nstring;
	int part;

	/* FIXME: We should handle the % values as floats. */

	(*string) += 4;
	skip_whitespace(*string);
	part = strtol(*string, (char **) &nstring, 10);
	if (*string == nstring) {
		return -1;
	}
	*string = nstring;
	skip_whitespace(*string);
	if (**string == '%') {
		part *= 255; part /= 100;
		(*string)++;
	}
	skip_whitespace(*string);
	if (**string != terminator) {
		return -1;
	}
	(*string)++;

	if (part > 255) part = 255;
	return part;
}

static int
css_parse_color_value(union css_decl_value *value, unsigned char **string)
{
	int pos;

	if (!strncasecmp(*string, "rgb(", 4)) {
		/* RGB function */
		int part;

		(*string) += 4;

		part = rgb_component_parser(string, ',');
		if (part < 0) return 0;
		value->color |= part << 16;

		part = rgb_component_parser(string, ',');
		if (part < 0) return 0;
		value->color |= part << 8;

		part = rgb_component_parser(string, ')');
		if (part < 0) return 0;
		value->color |= part;

		return 1;
	}

	/* Just a color value we already know how to parse. */
	
	pos = strcspn(*string, ",; \t\r\n");
	if (decode_color(*string, pos, &value->color) < 0) {
		return 0;
	}
	string += pos;
	return 1;
}


static int
css_parse_font_attribute_value(union css_decl_value *value,
				unsigned char **string)
{
	unsigned char *nstring;
	int weight;

	/* This is triggered with a lot of various properties, basically
	 * everything just touching font_attribute. */

	/* font-weight */

	if (!strncasecmp(*string, "bolder", 6)) {
		(*string) += 6;
		value->font_attribute.add |= AT_BOLD;
		return 1;
	}

	if (!strncasecmp(*string, "lighter", 7)) {
		(*string) += 7;
		value->font_attribute.rem |= AT_BOLD;
		return 1;
	}

	if (!strncasecmp(*string, "bold", 4)) {
		(*string) += 4;
		value->font_attribute.add |= AT_BOLD;
		return 1;
	}

	if (!strncasecmp(*string, "normal", 6)) {
		(*string) += 6;
		/* XXX: font-weight/font-style distinction */
		value->font_attribute.rem |= AT_BOLD | AT_ITALIC;
		return 1;
	}

	/* font-style */

	if (!strncasecmp(*string, "italic", 6) ||
	    !strncasecmp(*string, "oblique", 7)) {
		(*string) += 6 + (**string == 'o');
		value->font_attribute.add |= AT_ITALIC;
		return 1;
	}

	/* font-weight II. */

	/* TODO: Comma separated list of weights?! */
	weight = strtol(*string, (char **) &nstring, 10);
	if (*string == nstring) {
		return 0;
	}

	*string = nstring;
	/* The font weight(s) have values between 100 to 900.  These
	 * values form an ordered sequence, where each number indicates
	 * a weight that is at least as dark as its predecessor.
	 *
	 * normal -> Same as '400'.  bold Same as '700'.
	 */
	int_bounds(&weight, 100, 900);
	if (weight >= 700) value->font_attribute.add |= AT_BOLD;
	return 1;
}


static int
css_parse_text_align_value(union css_decl_value *value, unsigned char **string)
{
	if (!strncasecmp(*string, "left", 4)) {
		(*string) += 4;
		value->text_align = AL_LEFT;
		return 1;
	}

	if (!strncasecmp(*string, "right", 5)) {
		(*string) += 5;
		value->text_align = AL_RIGHT;
		return 1;
	}

	if (!strncasecmp(*string, "center", 6)) {
		(*string) += 6;
		value->text_align = AL_CENTER;
		return 1;
	}

	if (!strncasecmp(*string, "justify", 7)) {
		(*string) += 7;
		value->text_align = AL_BLOCK;
		return 1;
	}

	return 0;
}


static css_value_parser_t css_value_parsers[CSS_DV_LAST] = {
	/* CSS_DV_NONE */		NULL,
	/* CSS_DV_COLOR */		css_parse_color_value,
	/* CSS_DV_FONT_ATTRIBUTE */	css_parse_font_attribute_value,
	/* CSS_DV_TEXT_ALIGN */		css_parse_text_align_value,
};

int
css_parse_value(enum css_decl_valtype valtype, union css_decl_value *value,
		unsigned char **string)
{
	assert(string && value && valtype < CSS_DV_LAST);
	assert(css_value_parsers[valtype]);

	/* Skip the leading whitespaces. */
	skip_whitespace(*string);

	return css_value_parsers[valtype](value, string);
}
