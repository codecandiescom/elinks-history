/* $Id: css.h,v 1.5 2004/01/17 15:36:20 pasky Exp $ */

#ifndef EL__DOCUMENT_CSS_CSS_H
#define EL__DOCUMENT_CSS_CSS_H

#include "document/html/parser.h"
#include "util/color.h"
#include "util/lists.h"

struct html_element;

/* This is a super-simplistic CSS micro-engine. */

/* What Is Gonna Be A FAQ: Why isn't the CSS support optional? */
/* Answer: Because I want to eliminate all trivial styling (like html_h*())
 * and substitute it with default stylesheet information, so that this can be
 * user-configured. On the other side we might yet reconsider whether we want
 * that before 1.0. Maybe we don't. */

/* TODO: Its job is separated to two phases. The first phase is a parser, it
 * takes a string with CSS code and transforms it to an internal set of
 * structures describing the data (let's call it a "rawer"). The second phase
 * is an applier, which applies given rawer to the current element. --pasky */

/* Currently only the element's 'style' attribute is checked, therefore the
 * first stage is not exported yet (it'd be useless for that). Only the second
 * stage is available now, and it doesn't take the stylesheet argument yet.  It
 * will automatically scan the current element, and if a 'style' attribute is
 * found, it is parsed and applied to the current element. */


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
