/* $Id: apply.h,v 1.9 2004/01/19 17:03:49 jonas Exp $ */

#ifndef EL__DOCUMENT_CSS_APPLY_H
#define EL__DOCUMENT_CSS_APPLY_H

struct css_stylesheet;
struct html_element;

/* This is the main entry point for the CSS micro-engine. It throws all the
 * power of the stylesheets at a given element. */

/* This function takes @element and applies its 'style' attribute onto its
 * attributes (if it contains such an attribute). */
void css_apply(struct html_element *element, struct css_stylesheet *css);

#endif
