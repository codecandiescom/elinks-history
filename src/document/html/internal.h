/* $Id: internal.h,v 1.4 2004/04/24 00:03:58 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_INTERNAL_H
#define EL__DOCUMENT_HTML_INTERNAL_H

#include "document/html/parser.h"
#include "util/lists.h"

extern struct list_head html_stack;

#define format (((struct html_element *) html_stack.next)->attr)
#define par_format (((struct html_element *) html_stack.next)->parattr)
#define html_top (*(struct html_element *) html_stack.next)

extern void *ff;
extern void (*line_break_f)(void *);

void ln_break(int n, void (*line_break)(void *), void *f);

#endif
