/* $Id: stack.h,v 1.13 2005/07/10 23:07:29 miciah Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_STACK_H
#define EL__DOCUMENT_HTML_PARSER_STACK_H

#include "document/html/parser.h"

struct html_context;

struct html_element *search_html_stack(struct html_context *html_context,
                                       unsigned char *name);

void html_stack_dup(enum html_element_type type,
                    struct html_context *html_context);

void kill_html_stack_item(struct html_context *html_context,
                          struct html_element *e);
void kill_html_stack_until(struct html_context *html_context, int ls, ...);

/* void dump_html_stack(struct html_context *html_context); */

#endif
