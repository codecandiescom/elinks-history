/* CSS property value parser */
/* $Id: value.c,v 1.28 2004/01/18 17:48:11 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/property.h"
#include "document/css/scanner.h"
#include "document/css/value.h"
#include "document/html/parser.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


static int
rgb_component_parser(unsigned char **string, unsigned char terminator)
{
	unsigned char *nstring;
	int part;

	/* FIXME: We should handle the % values as floats. */

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
do_css_parse_color_value(struct css_property_info *propinfo,
		      union css_property_value *value,
		      unsigned char **string)
{
	int pos;

	assert(propinfo->value_type == CSS_VT_COLOR);

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
		(*string) += pos;
		return 0;
	}
	(*string) += pos;
	return 1;
}

int
css_parse_color_value(struct css_property_info *propinfo,
		      union css_property_value *value,
		      struct css_scanner *scanner)
{
	struct css_token *token = get_css_token(scanner);
	unsigned char *string = token->string;

	return do_css_parse_color_value(propinfo, value, &string);
}

int
css_parse_background_value(struct css_property_info *propinfo,
			   union css_property_value *value,
			   struct css_scanner *scanner)
{
	struct css_token *token = get_css_token(scanner);
	unsigned char *tok_string = token->string;
	unsigned char **string = &tok_string;
	int success = 0;

	assert(propinfo->value_type == CSS_VT_COLOR);

	/* This is pretty naive, we just jump space by space, trying to parse
	 * each token as a color. */

	while (**string && **string != ';') {
		success += do_css_parse_color_value(propinfo, value, string);
		if (**string == ',')
			(*string)++; /* Uh. */
		skip_whitespace(*string);
	}

	return success;
}


int
css_parse_font_style_value(struct css_property_info *propinfo,
			   union css_property_value *value,
			   struct css_scanner *scanner)
{
	struct css_token *token = get_css_token(scanner);
	unsigned char *tok_string = token->string;
	unsigned char **string = &tok_string;

	assert(propinfo->value_type == CSS_VT_FONT_ATTRIBUTE);

	if (!strncasecmp(*string, "normal", 6)) {
		(*string) += 6;
		value->font_attribute.rem |= AT_ITALIC;
		return 1;
	}

	if (!strncasecmp(*string, "italic", 6) ||
	    !strncasecmp(*string, "oblique", 7)) {
		(*string) += 6 + (**string == 'o');
		value->font_attribute.add |= AT_ITALIC;
		return 1;
	}

	return 0;
}


int
css_parse_font_weight_value(struct css_property_info *propinfo,
			    union css_property_value *value,
			    struct css_scanner *scanner)
{
	struct css_token *token = get_css_token(scanner);
	unsigned char *tok_string = token->string;
	unsigned char **string = &tok_string;
	unsigned char *nstring;
	int weight;

	assert(propinfo->value_type == CSS_VT_FONT_ATTRIBUTE);

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
		value->font_attribute.rem |= AT_BOLD;
		return 1;
	}

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


int
css_parse_text_align_value(struct css_property_info *propinfo,
			   union css_property_value *value,
			   struct css_scanner *scanner)
{
	struct css_token *token = get_css_token(scanner);

	assert(propinfo->value_type == CSS_VT_TEXT_ALIGN);

	if (token->type != CSS_TOKEN_IDENTIFIER) return 0;

	if (css_token_contains(token, "left", 4)) {
		value->text_align = AL_LEFT;

	} else 	if (css_token_contains(token, "right", 5)) {
		value->text_align = AL_RIGHT;

	} else 	if (css_token_contains(token, "center", 6)) {
		value->text_align = AL_CENTER;

	} else 	if (css_token_contains(token, "justify", 7)) {
		value->text_align = AL_BLOCK;

	} else {
		return 0;
	}

	skip_css_tokens(scanner, CSS_TOKEN_IDENTIFIER);
	return 1;
}


int
css_parse_value(struct css_property_info *propinfo,
		union css_property_value *value,
		struct css_scanner *scanner)
{
	struct css_token *token;

	assert(scanner && value && propinfo);
	assert(propinfo->parser);

	/* Check that we actually have some token to pass on */
	token = get_css_token(scanner);
	if (!token) return 0;

	return propinfo->parser(propinfo, value, scanner);
}
