/* HTML elements stack */
/* $Id: stack.c,v 1.17 2004/06/23 10:33:06 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/parser/stack.h"
#include "document/html/parser.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"


#if 0
void
dump_html_stack(void)
{
	struct html_element *element;

	DBG("HTML stack debug:");
	foreach (element, html_context.stack) {
		DBG("&name/len:%p:%d name:%.*s type:%d",
		    element->name, element->namelen,
		    element->namelen, element->name,
		    element->type);
	}
	WDBG("Did you enjoy it?");
}
#endif


struct html_element *
search_html_stack(unsigned char *name)
{
	struct html_element *element;
	int namelen;

	assert(name && *name);
	namelen = strlen(name);

#if 0	/* Debug code. Please keep. */
	dump_html_stack();
#endif

	foreach (element, html_context.stack) {
		if (element == &html_top)
			continue; /* skip the top element */
		if (strlcasecmp(element->name, element->namelen, name, namelen))
			continue;
		return element;
	}

	return NULL;
}


void
kill_html_stack_item(struct html_element *e)
{
	assert(e);
	if_assert_failed return;
	assertm((void *) e != &html_context.stack, "trying to free bad html element");
	if_assert_failed return;
	assertm(e->type != ELEMENT_IMMORTAL, "trying to kill unkillable element");
	if_assert_failed return;

	mem_free_if(e->attr.link);
	mem_free_if(e->attr.target);
	mem_free_if(e->attr.image);
	mem_free_if(e->attr.title);
	mem_free_if(e->attr.target_base);
	mem_free_if(e->attr.select);
	done_uri(e->attr.href_base);
	del_from_list(e);
	mem_free(e);
#if 0
	if (list_empty(html_context.stack)
	    || !html_context.stack.next) {
		DBG("killing last element");
	}
#endif
}


void
html_stack_dup(enum html_element_type type)
{
	struct html_element *e;
	struct html_element *ep = html_context.stack.next;

	assertm(ep && (void *) ep != &html_context.stack, "html stack empty");
	if_assert_failed return;

	e = mem_alloc(sizeof(struct html_element));
	if (!e) return;

	memcpy(e, ep, sizeof(struct html_element));

	if (ep->attr.link) e->attr.link = stracpy(ep->attr.link);
	if (ep->attr.target) e->attr.target = stracpy(ep->attr.target);
	if (ep->attr.image) e->attr.image = stracpy(ep->attr.image);
	if (ep->attr.title) e->attr.title = stracpy(ep->attr.title);
	if (ep->attr.target_base) e->attr.target_base = stracpy(ep->attr.target_base);
	if (ep->attr.select) e->attr.select = stracpy(ep->attr.select);

#if 0
	if (e->name) {
		if (e->attr.link) set_mem_comment(e->attr.link, e->name, e->namelen);
		if (e->attr.target) set_mem_comment(e->attr.target, e->name, e->namelen);
		if (e->attr.image) set_mem_comment(e->attr.image, e->name, e->namelen);
		if (e->attr.title) set_mem_comment(e->attr.title, e->name, e->namelen);
		if (e->attr.href_base) set_mem_comment(e->attr.href_base, e->name, e->namelen);
		if (e->attr.target_base) set_mem_comment(e->attr.target_base, e->name, e->namelen);
		if (e->attr.select) set_mem_comment(e->attr.select, e->name, e->namelen);
	}
#endif

	e->attr.href_base = get_uri_reference(ep->attr.href_base);
	e->name = e->options = NULL;
	e->namelen = 0;
	e->type = type;

	add_to_list(html_context.stack, e);
}


void
kill_html_stack_until(int ls, ...)
{
	int l;
	struct html_element *e = &html_top;

	if (ls) e = e->next;

	while ((void *) e != &html_context.stack) {
		int sk = 0;
		va_list arg;

		va_start(arg, ls);
		while (1) {
			unsigned char *s = va_arg(arg, unsigned char *);
			int slen;

			if (!s) break;

			slen = strlen(s);
			if (!slen) {
				sk++;
				continue;
			}

			if (strlcasecmp(e->name, e->namelen, s, slen))
				continue;

			if (!sk) {
				if (e->type < ELEMENT_KILLABLE) break;
				va_end(arg);
				goto killll;

			} else if (sk == 1) {
				va_end(arg);
				e = e->prev;
				goto killll;

			} else {
				break;
			}
		}
		va_end(arg);

		if (e->type < ELEMENT_KILLABLE
		    || (!strlcasecmp(e->name, e->namelen, "TABLE", 5)))
			break;

		if (e->namelen == 2 && toupper(e->name[0]) == 'T') {
			unsigned char c = toupper(e->name[1]);

			if (c == 'D' || c == 'H' || c == 'R')
				break;
		}

		e = e->next;
	}

	return;

killll:
	l = 0;
	while ((void *) e != &html_context.stack) {
		if (ls && e == html_context.stack.next)
			break;

		if (e->linebreak > l)
			l = e->linebreak;
		e = e->prev;
		kill_html_stack_item(e->next);
	}

	ln_break(l, html_context.line_break_f, html_context.ff);
}
