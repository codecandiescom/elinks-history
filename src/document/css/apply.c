/* CSS style applier */
/* $Id: apply.c,v 1.24 2004/01/17 20:24:09 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/apply.h"
#include "document/css/parser.h"
#include "document/css/property.h"
#include "document/html/parser.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/error.h"
#include "util/memory.h"


/* TODO: A way to disable CSS completely, PLUS a way to stop various property
 * groups from taking effect. (Ie. way to turn out effect of 'display: none'
 * or aligning or colors but keeping all the others.) --pasky */


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
css_apply_font_attribute(struct html_element *element, struct css_property *prop)
{
	assert(prop->value_type == CSS_DV_FONT_ATTRIBUTE);
	element->attr.attr |= prop->value.font_attribute.add;
	element->attr.attr &= ~prop->value.font_attribute.rem;
}

/* XXX: Sort like the css_decl_property */
static css_applier_t css_appliers[CSS_DP_LAST] = {
	/* CSS_DP_NONE */		NULL,
	/* CSS_DP_BACKGROUND_COLOR */	css_apply_background_color,
	/* CSS_DP_COLOR */		css_apply_color,
	/* CSS_DP_FONT_STYLE */		css_apply_font_attribute,
	/* CSS_DP_FONT_WEIGHT */	css_apply_font_attribute,
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
