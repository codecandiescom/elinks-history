/* $Id: property.h,v 1.3 2004/01/17 20:24:09 pasky Exp $ */

#ifndef EL__DOCUMENT_CSS_PROPERTY_H
#define EL__DOCUMENT_CSS_PROPERTY_H

#include "document/html/parser.h"
#include "util/color.h"
#include "util/lists.h"

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
		CSS_DP_FONT_STYLE,
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
		struct {
			enum format_attr add, rem;
		} font_attribute;
		/* TODO:
		 * Generic numbers
		 * Percentages
		 * URL
		 * Align (struct format_align) */
		/* TODO: The size units will be fun yet. --pasky */
	} value;
};

#endif
