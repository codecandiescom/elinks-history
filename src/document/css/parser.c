/* CSS main parser */
/* $Id: parser.c,v 1.8 2004/01/18 01:57:33 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/parser.h"
#include "document/css/property.h"
#include "document/css/value.h"
#include "document/html/parser.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


struct css_property_info {
	unsigned char *name;
	int namelen;
	enum css_decl_property property;
	enum css_decl_valtype value_type;
};

#define CSS_PROPERTY(name, property, valtype) \
	{ name, sizeof(name) - 1, property, valtype }

/* TODO: Use fastfind when we get a lot of properties. */
struct css_property_info css_property_info[] = {
	CSS_PROPERTY("background-color", CSS_DP_BACKGROUND_COLOR, CSS_DV_COLOR),
	CSS_PROPERTY("color", CSS_DP_COLOR, CSS_DV_COLOR),
	CSS_PROPERTY("font-style", CSS_DP_FONT_STYLE, CSS_DV_FONT_ATTRIBUTE),
	CSS_PROPERTY("font-weight", CSS_DP_FONT_WEIGHT, CSS_DV_FONT_ATTRIBUTE),
	CSS_PROPERTY("text-align", CSS_DP_TEXT_ALIGN, CSS_DV_TEXT_ALIGN),

	CSS_PROPERTY("", CSS_DP_NONE, CSS_DV_NONE),
};

void
css_parse_decl(struct list_head *props, unsigned char *string)
{
	struct css_property_info *property_info = NULL;
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
	if (string[pos] == 0) return;

	for (i = 0; css_property_info[i].namelen; i++) {
		struct css_property_info *info = &css_property_info[i];

		if (!strlcasecmp(string, pos, info->name, info->namelen)) {
			property_info = info;
			break;
		}
	}

	string += pos + 1;

	if (!property_info) {
		/* Unknown property, check the next one. */
ride_on:
		pos = strcspn(string, ";");
		if (string[pos] == ';') {
			return css_parse_decl(props, string + pos + 1);
		} else {
			return;
		}
	}

	/* We might be on track of something, cook up the struct. */

	prop = mem_calloc(1, sizeof(struct css_property));
	if (!prop) {
		goto ride_on;
	}
	prop->property = property_info->property;
	prop->value_type = property_info->value_type;
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
}
