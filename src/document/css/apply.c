/* CSS style applier */
/* $Id: apply.c,v 1.76 2004/09/21 09:58:00 pasky Exp $ */

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
#include "document/html/parser/parse.h"
#include "document/options.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* #define CSS_DEBUG */


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
	/* CSS_PT_WHITE_SPACE */	css_apply_font_attribute,
};

/* This looks for a match in list of selectors. */
static void
examine_element(struct css_selector *base,
		enum css_selector_type seltype, enum css_selector_relation rel,
                struct list_head *selectors, struct html_element *element)
{
	struct css_selector *selector;
	unsigned char *code;

#ifdef CSS_DEBUG
 	DBG("examine_element(%s, %d, %d, %p, %.*s);", base->name, seltype, rel, selectors, element->namelen, element->name);
#define dbginfo(sel, type_, base) \
	DBG("Matched selector %s (rel %d type %d [m%d])! Children %p !!%d, props !!%d", sel->name, sel->relation, sel->type, sel->type == type_, sel->leaves, !list_empty(sel->leaves), !list_empty(sel->properties))
#else
#define dbginfo(sel, type, base)
#endif

#define process_found_selector(sel, type, base) \
	if (selector) { \
		dbginfo(sel, type, base); \
		merge_css_selectors(base, sel); \
		/* More specific matches? */ \
		examine_element(sel, type + 1, CSR_SPECIFITY, \
		                &sel->leaves, element); \
		/* TODO: HTML stack lookup. */ \
	}

	if (seltype <= CST_ELEMENT) {
		selector = find_css_selector(selectors, CST_ELEMENT,
		                             element->name, element->namelen);
		process_found_selector(selector, CST_ELEMENT, base);
	}

	code = get_attr_val(element->options, "id");
	if (code && seltype <= CST_ID) {
		selector = find_css_selector(selectors, CST_ID, code, -1);
		process_found_selector(selector, CST_ID, base);
	}
	if (code) mem_free(code);

	code = get_attr_val(element->options, "class");
	if (code && seltype <= CST_CLASS) {
		selector = find_css_selector(selectors, CST_CLASS, code, -1);
		process_found_selector(selector, CST_CLASS, base);
	}
	if (code) mem_free(code);

	/* TODO: Somehow handle pseudo-classess. The css_apply() caller will
	 * have to tell us about those. --pasky */

#undef process_found_selector
#undef dbginfo
}

void
css_apply(struct html_element *element, struct css_stylesheet *css)
{
	INIT_LIST_HEAD(props);
	unsigned char *code;
	struct css_property *property;
	struct css_selector *selector;

	assert(element && element->options && css);

	selector = init_css_selector(NULL, CST_ELEMENT, NULL, 0);
	if (!selector)
		return;

#ifdef CSS_DEBUG
	WDBG("Applying to element %.*s...", element->namelen, element->name);
#endif

	examine_element(selector, CST_ELEMENT, CSR_ROOT,
	                &css->selectors, element);

#ifdef CSS_DEBUG
	WDBG("Element %.*s applied.", element->namelen, element->name);
#endif

	code = get_attr_val(element->options, "style");
	if (code) {
		struct css_selector *stylesel;
		struct scanner scanner;

		stylesel = init_css_selector(NULL, CST_ELEMENT, NULL, 0);

		if (stylesel) {
			init_scanner(&scanner, &css_scanner_info, code, NULL);
			css_parse_properties(&stylesel->properties, &scanner);
			merge_css_selectors(selector, stylesel);
			done_css_selector(stylesel);
		}
		mem_free(code);
	}

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
