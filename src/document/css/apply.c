/* CSS style applier */
/* $Id: apply.c,v 1.55 2004/03/09 15:05:54 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/apply.h"
#include "document/css/css.h"
#include "document/css/parser.h"
#include "document/css/property.h"
#include "document/css/scanner.h"
#include "document/css/stylesheet.h"
#include "document/html/parser.h"
#include "document/options.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: A way to disable CSS completely, PLUS a way to stop various property
 * groups from taking effect. (Ie. way to turn out effect of 'display: none'
 * or aligning or colors but keeping all the others.) --pasky */


typedef void (*css_applier_t)(struct html_element *element,
			      struct css_property *prop);

static void
css_apply_color(struct html_element *element, struct css_property *prop)
{
	assert(prop->value_type == CSS_VT_COLOR);

	if (use_document_fg_colors(global_doc_opts))
		element->attr.fg = prop->value.color;
}

static void
css_apply_background_color(struct html_element *element,
			   struct css_property *prop)
{
	assert(prop->value_type == CSS_VT_COLOR);

	if (use_document_bg_colors(global_doc_opts))
		element->attr.bg = prop->value.color;
}

static void
css_apply_font_attribute(struct html_element *element, struct css_property *prop)
{
	assert(prop->value_type == CSS_VT_FONT_ATTRIBUTE);
	element->attr.attr |= prop->value.font_attribute.add;
	element->attr.attr &= ~prop->value.font_attribute.rem;
}

/* FIXME: Because the current CSS doesn't provide reasonable defaults for each
 * HTML element this applier will cause bad rendering of <pre> tags. */
static void
css_apply_text_align(struct html_element *element, struct css_property *prop)
{
	assert(prop->value_type == CSS_VT_TEXT_ALIGN);
	element->parattr.align = prop->value.text_align;
}

/* XXX: Sort like the css_property_type */
static css_applier_t css_appliers[CSS_PT_LAST] = {
	/* CSS_PT_NONE */		NULL,
	/* CSS_PT_BACKGROUND */		css_apply_background_color,
	/* CSS_PT_BACKGROUND_COLOR */	css_apply_background_color,
	/* CSS_PT_COLOR */		css_apply_color,
	/* CSS_PT_FONT_STYLE */		css_apply_font_attribute,
	/* CSS_PT_FONT_WEIGHT */	css_apply_font_attribute,
	/* CSS_PT_TEXT_ALIGN */		css_apply_text_align,
	/* CSS_PT_TEXT_DECORATION */	css_apply_font_attribute,
};

void
css_apply(struct html_element *element, struct css_stylesheet *css)
{
	INIT_LIST_HEAD(props);
	unsigned char *code;
	struct css_property *property;
	struct css_selector *selector, *altsel;

	assert(element && element->options && css);

	selector = init_css_selector(NULL, NULL, 0);
	if (!selector)
		return;

	code = get_attr_val(element->options, "style");
	if (code) {
		struct scanner scanner;

		init_scanner(&scanner, &css_scanner_info, code, NULL);
		css_parse_properties(&selector->properties, &scanner);
		mem_free(code);
	}

	altsel = find_css_selector(css, element->name,
				   element->namelen);
	if (altsel) merge_css_selectors(selector, altsel);

	foreach (property, selector->properties) {
		assert(property->type < CSS_PT_LAST);
		/* We don't assert general prop->value_type here because I
		 * don't want hinder properties' ability to potentially make
		 * use of multiple value types. */
		assert(css_appliers[property->type]);
		css_appliers[property->type](element, property);
	}

	done_css_selector(selector);
}
