/* $Id: stack.h,v 1.7 2005/07/09 20:34:30 miciah Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_STACK_H
#define EL__DOCUMENT_HTML_PARSER_STACK_H

#include "document/html/parser.h"

struct html_context;

struct html_element *search_html_stack(unsigned char *name,
                                       struct html_context *html_context);

void html_stack_dup(enum html_element_type type);

void kill_html_stack_item(struct html_element *e);
void kill_html_stack_until(int ls, struct html_context *html_context, ...);

/* void dump_html_stack(void); */

#endif
