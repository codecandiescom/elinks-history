/* $Id: apply.h,v 1.8 2004/01/17 16:36:16 pasky Exp $ */

#ifndef EL__DOCUMENT_CSS_APPLY_H
#define EL__DOCUMENT_CSS_APPLY_H

struct html_element;

/* This is the main entry point for the CSS micro-engine. It throws all the
 * power of the stylesheets at a given element. */

/* This function takes @element and applies its 'style' attribute onto its
 * attributes (if it contains such an attribute). */
void css_apply(struct html_element *element);

#endif
