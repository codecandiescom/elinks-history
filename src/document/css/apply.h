/* $Id: apply.h,v 1.7 2004/01/17 16:26:21 pasky Exp $ */

#ifndef EL__DOCUMENT_CSS_APPLY_H
#define EL__DOCUMENT_CSS_APPLY_H

#include "document/html/parser.h"
#include "util/color.h"
#include "util/lists.h"

struct html_element;

/* This is the main entry point for the CSS micro-engine. It throws all the
 * power of the stylesheets at a given element. */


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
		CSS_DP_FONT_WEIGHT,
		CSS_DP_LAST,
	} property;

	/* Property value. If it is a pointer, it points always to a memory
	 * to be free()d together with this structure. */

	enum css_decl_valtype {
		CSS_DV_NONE,
		CSS_DV_COLOR,
		CSS_DV_FONT_ATTRIBUTE,
		CSS_DV_LAST,
	} value_type;
	union css_decl_value {
		void *dummy;
		color_t color;
		enum format_attr font_attribute;
		/* TODO:
		 * Generic numbers
		 * Percentages
		 * URL
		 * Align (struct format_align) */
		/* TODO: The size units will be fun yet. --pasky */
	} value;
};


/* This function takes @element and applies its 'style' attribute onto its
 * attributes (if it contains such an attribute). */
void css_apply(struct html_element *element);

#endif
