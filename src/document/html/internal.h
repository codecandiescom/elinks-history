/* $Id: internal.h,v 1.3 2004/04/23 23:23:10 pasky Exp $ */

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

#endif
