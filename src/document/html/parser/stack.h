/* $Id: stack.h,v 1.3 2004/04/23 23:08:12 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_STACK_H
#define EL__DOCUMENT_HTML_PARSER_STACK_H

#include "document/html/parser.h"

struct html_element;

struct html_element *search_html_stack(unsigned char *name);

void html_stack_dup(enum html_element_type type);

void kill_html_stack_item(struct html_element *e);
void kill_html_stack_until(int ls, ...);

/* void debug_stack(void); */

#endif
