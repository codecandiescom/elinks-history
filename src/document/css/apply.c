/* CSS micro-engine */
/* $Id: apply.c,v 1.3 2004/01/17 02:24:10 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/options.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/parser.h"


/* TODO: Move this to document/css/ when it'll be big enough or
 * non-HTML-specific. --pasky */

/* TODO: A way to disable CSS completely, PLUS a way to stop various property
 * groups from taking effect. (Ie. way to turn out effect of 'display: none'
 * or aligning or colors but keeping all the others.) --pasky */


/* The {struct css_property} describes one CSS declaration in a rule. One list of
 * these contains all the declarations contained in one rule. */

struct css_property {
	LIST_HEAD(struct css_property);

	/* Declared property. The enum item name is derived from the property
	 * name, just uppercase it and tr/-/_/. */

	enum css_decl_property {
		CSS_DP_NONE,
		CSS_DP_BACKGROUND_COLOR,
		CSS_DP_COLOR,
		CSS_DP_LAST,
	} property;

	/* Property value. If it is a pointer, it points always to a memory
	 * to be free()d together with this structure. */

	enum css_decl_valtype {
		CSS_DV_NONE,
		CSS_DV_COLOR,
		CSS_DV_LAST,
	} value_type;
	union css_decl_value {
		void *dummy;
		color_t color;
		/* TODO:
		 * Generic numbers
		 * Percentages
		 * URL
		 * Align (struct format_align) */
		/* TODO: The size units will be fun yet. --pasky */
	} value;
};


/* Property <-> valtype associations. Indexed by property. */
static enum css_decl_valtype prop2valtype[CSS_DP_LAST] = {
	/* CSS_DP_NONE */		CSS_DV_NONE,
	/* CSS_DP_BACKGROUND_COLOR */	CSS_DV_COLOR,
	/* CSS_DP_COLOR */		CSS_DV_COLOR,
};


/* This function takes a value of a specified type from the given string and
 * converts it to a reasonable {struct css_property}-ready form. */
/* It returns positive integer upon success, zero upon parse error, and moves
 * the string pointer to the byte after the value end. */
static int
css_parse_value(enum css_decl_valtype valtype, union css_decl_value *value,
		unsigned char **string)
{
	assert(string && valtype != CSS_DV_NONE && valtype < CSS_DV_LAST);

	/* Skip the leading whitespaces. */
	skip_whitespace(*string);

	if (valtype == CSS_DV_COLOR) {
		int pos;

		/* Long live spaghetti code! Well, whatever, someone will come
		 * by and clean this up. I hope. Er... of course someone will!
		 * --pasky */

		/* TODO: Generic parser for "functions" in the declarations.
		 * --pasky */

		if (!strncasecmp(*string, "rgb(", 4)) {
			/* RGB function */
			unsigned char *nstring;
			int part;

			/* FIXME: We should handle the % values as floats. */

			(*string) += 4;
			skip_whitespace(*string);
			part = strtol(*string, (char **) &nstring, 10);
			if (*string == nstring) {
				return 0;
			}
			*string = nstring;
			skip_whitespace(*string);
			if (**string == '%') {
				part *= 255; part /= 100;
				(*string)++;
			}
			if (part > 255) part = 255;
			value->color |= part << 16;
			skip_whitespace(*string);
			if (**string != ',') {
				return 0;
			}
			(*string)++;

			skip_whitespace(*string);
			part = strtol(*string, (char **) &nstring, 10);
			if (*string == nstring) {
				return 0;
			}
			*string = nstring;
			skip_whitespace(*string);
			if (**string == '%') {
				part *= 255; part /= 100;
				(*string)++;
			}
			if (part > 255) part = 255;
			value->color |= part << 8;
			skip_whitespace(*string);
			if (**string != ',') {
				return 0;
			}
			(*string)++;

			skip_whitespace(*string);
			part = strtol(*string, (char **) &nstring, 10);
			if (*string == nstring) {
				return 0;
			}
			*string = nstring;
			skip_whitespace(*string);
			if (**string == '%') {
				part *= 255; part /= 100;
				(*string)++;
			}
			if (part > 255) part = 255;
			value->color |= part;
			skip_whitespace(*string);
			if (**string != ')') {
				return 0;
			}
			(*string)++;

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

	INTERNAL("Uh-oh. I the %d am not supposed to be here.", valtype);
	return 0;
}


/* This function takes a declaration from the given string, parses it to atoms,
 * and possibly creates {struct css_property} and chains it up to the specified
 * list. The function returns positive value in case it recognized a property
 * in the given string, or zero in case of an error. */
/* This function is recursive, therefore if you give it a string containing
 * multiple declarations separated by a semicolon, it will call itself for each
 * of the following declarations. Then it returns success in case at least one
 * css_parse_decl() run succeeded. In case of failure, it tries to do an error
 * recovery by simply looking at the nearest semicolon ahead. */
static int
css_parse_decl(struct list_head *props, unsigned char *string)
{
	enum css_decl_property property = CSS_DP_NONE;
	struct css_property *prop;
	int pos;

	assert(props && string);

	/* Align myself. */

	skip_whitespace(string);

	/* Extract property name. */

	pos = strcspn(string, ":;");
	if (string[pos] == ';') {
		return css_parse_decl(props, string + pos + 1);
	}
	if (string[pos] == 0) {
		return 0;
	}

	if (!strlcasecmp(string, pos, "color", 5)) {
		property = CSS_DP_COLOR;
	} else if (!strlcasecmp(string, pos, "background-color", 16)) {
		property = CSS_DP_BACKGROUND_COLOR;
	}

	string += pos + 1;

	if (property == CSS_DP_NONE) {
		/* Unknown property, check the next one. */
ride_on:
		pos = strcspn(string, ";");
		if (string[pos] == ';') {
			return css_parse_decl(props, string + pos + 1);
		} else {
			return 0;
		}
	}

	/* We might be on track of something, cook up the struct. */

	prop = mem_calloc(1, sizeof(struct css_property));
	if (!prop) {
		goto ride_on;
	}
	prop->property = property;
	prop->value_type = prop2valtype[property];
	if (!css_parse_value(prop->value_type, &prop->value, &string)) {
		mem_free(prop);
		goto ride_on;
	}
	add_to_list(*props, prop);

	/* Maybe we have something else to go yet? */

	pos = strcspn(string, ";");
	if (string[pos] == ';') {
		css_parse_decl(props, string + pos + 1);
	}
	return 1;
}


int
css_apply(struct html_element *element)
{
	INIT_LIST_HEAD(props);
	unsigned char *code;
	struct css_property *prop;
	int i = 0;

	assert(element && element->options);

	code = get_attr_val(element->options, "style");
	if (!code)
		return 0;

	css_parse_decl(&props, code);
	mem_free(code);

	foreach (prop, props) {
		i++;
		switch (prop->property) {
			case CSS_DP_BACKGROUND_COLOR:
				assert(prop->value_type == CSS_DV_COLOR);
				element->attr.bg = prop->value.color;
				break;
			case CSS_DP_COLOR:
				assert(prop->value_type == CSS_DV_COLOR);
				element->attr.fg = prop->value.color;
				break;
			default:
				INTERNAL("Unknown property %d!",
					 prop->property);
		}
	}

	while (!list_empty(props)) {
		struct css_property *prop = props.next;

		del_from_list(prop);
		mem_free(prop);
	}

	return i;
}
