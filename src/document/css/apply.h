/* $Id: apply.h,v 1.10 2004/09/21 15:03:01 pasky Exp $ */

#ifndef EL__DOCUMENT_CSS_APPLY_H
#define EL__DOCUMENT_CSS_APPLY_H

#include "util/lists.h"

struct css_stylesheet;
struct html_element;

/* This is the main entry point for the CSS micro-engine. It throws all the
 * power of the stylesheets at a given element. */

/* This function takes @element and applies its 'style' attribute onto its
 * attributes (if it contains such an attribute). */
void css_apply(struct html_element *element, struct css_stylesheet *css,
               struct list_head *html_stack);

#endif
