/* CSS micro-engine */
/* $Id: apply.c,v 1.18 2004/01/17 15:45:48 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/css.h"
#include "document/css/value.h"
#include "document/html/parser.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: A way to disable CSS completely, PLUS a way to stop various property
 * groups from taking effect. (Ie. way to turn out effect of 'display: none'
 * or aligning or colors but keeping all the others.) --pasky */


/* Property <-> valtype associations. Indexed by property. */
static enum css_decl_valtype prop2valtype[CSS_DP_LAST] = {
	/* CSS_DP_NONE */		CSS_DV_NONE,
	/* CSS_DP_BACKGROUND_COLOR */	CSS_DV_COLOR,
	/* CSS_DP_COLOR */		CSS_DV_COLOR,
	/* CSS_DP_FONT_WEIGHT */	CSS_DV_FONT_ATTRIBUTE,
};


struct css_property_info {
	unsigned char *name;
	int namelen;
	enum css_decl_property property;
};

#define CSS_PROPERTY(name, property) { name, sizeof(name) - 1, property }

/* TODO: Use fastfind when we get a lot of properties. */
struct css_property_info css_property_info[] = {
	CSS_PROPERTY("color", CSS_DP_COLOR),
	CSS_PROPERTY("background-color", CSS_DP_BACKGROUND_COLOR),
	CSS_PROPERTY("font-weight", CSS_DP_FONT_WEIGHT),

	CSS_PROPERTY("", CSS_DP_NONE),
};

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
	int pos, i;

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

	for (i = 0; css_property_info[i].namelen; i++) {
		struct css_property_info *info = &css_property_info[i];

		if (!strlcasecmp(string, pos, info->name, info->namelen)) {
			property = info->property;
			break;
		}
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


typedef void (*css_applier_t)(struct html_element *element,
			      struct css_property *prop);

static void
css_apply_color(struct html_element *element, struct css_property *prop)
{
	assert(prop->value_type == CSS_DV_COLOR);
	element->attr.fg = prop->value.color;
}

static void
css_apply_background_color(struct html_element *element,
			   struct css_property *prop)
{
	assert(prop->value_type == CSS_DV_COLOR);
	element->attr.bg = prop->value.color;
}

static void
css_apply_font_weight(struct html_element *element, struct css_property *prop)
{
	assert(prop->value_type == CSS_DV_FONT_ATTRIBUTE);
	element->attr.attr |= prop->value.font_attribute;
}

static css_applier_t css_appliers[CSS_DP_LAST] = {
	/* CSS_DP_NONE */		NULL,
	/* CSS_DP_COLOR */		css_apply_color,
	/* CSS_DP_BACKGROUND_COLOR */	css_apply_background_color,
	/* CSS_DP_FONT_WEIGHT */	css_apply_font_weight,
};

void
css_apply(struct html_element *element)
{
	INIT_LIST_HEAD(props);
	unsigned char *code;
	struct css_property *prop;

	assert(element && element->options);

	code = get_attr_val(element->options, "style");
	if (!code)
		return;

	css_parse_decl(&props, code);
	mem_free(code);

	foreach (prop, props) {
		assert(prop->property < CSS_DP_LAST);
		/* We don't assert general prop->value_type here because I
		 * don't want hinder properties' ability to potentially make
		 * use of multiple value types. */
		assert(css_appliers[prop->property]);
		css_appliers[prop->property](element, prop);
	}

	while (!list_empty(props)) {
		struct css_property *prop = props.next;

		del_from_list(prop);
		mem_free(prop);
	}
}
