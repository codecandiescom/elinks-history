/* HTML elements stack */
/* $Id: stack.c,v 1.2 2004/04/23 23:06:40 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "document/css/stylesheet.h"
#include "document/html/frames.h"
#include "document/html/parser/stack.h"
#include "document/html/parser.h"
#include "intl/charsets.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"


INIT_LIST_HEAD(html_stack);


#if 0
static void
dump_html_stack()
{
	struct html_element *element;

	foreach (element, html_stack) {
		DBG(":%p:%d:%.*s", element->name, element->namelen,
				element->namelen, element->name);
	}
	WDBG("Did you enjoy it?");
}
#endif

struct html_element *
search_html_stack(char *name)
{
	struct html_element *element;
	int namelen;

	assert(name && *name);
	namelen = strlen(name);

#if 0	/* Debug code. Please keep. */
	dump_html_stack();
#endif

	foreach (element, html_stack) {
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
	assertm((void *)e != &html_stack, "trying to free bad html element");
	if_assert_failed return;
	assertm(e->type != ELEMENT_IMMORTAL, "trying to kill unkillable element");
	if_assert_failed return;

	mem_free_if(e->attr.link);
	mem_free_if(e->attr.target);
	mem_free_if(e->attr.image);
	mem_free_if(e->attr.title);
	mem_free_if(e->attr.href_base);
	mem_free_if(e->attr.target_base);
	mem_free_if(e->attr.select);
	del_from_list(e);
	mem_free(e);
#if 0
	if (list_empty(html_stack) || !html_stack.next) {
		DBG("killing last element");
	}
#endif
}

#if 0
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
/* Never called */
void
debug_stack(void)
{
	struct html_element *e;

	printf("HTML stack debug: \n");
	foreachback (e, html_stack) {
		int i;

		printf("\"");
		for (i = 0; i < e->namelen; i++) printf("%c", e->name[i]);
		printf("\" : %d", e->type);
		printf("\n");
	}
	printf("%c", 7);
	fflush(stdout);
	sleep(1);
}
#endif

void
html_stack_dup(enum html_element_type type)
{
	struct html_element *e;
	struct html_element *ep = html_stack.next;

	assertm(ep && (void *)ep != &html_stack, "html stack empty");
	if_assert_failed return;

	e = mem_alloc(sizeof(struct html_element));
	if (!e) return;
	memcpy(e, ep, sizeof(struct html_element));
	if (ep->attr.link) e->attr.link = stracpy(ep->attr.link);
	if (ep->attr.target) e->attr.target = stracpy(ep->attr.target);
	if (ep->attr.image) e->attr.image = stracpy(ep->attr.image);
	if (ep->attr.title) e->attr.title = stracpy(ep->attr.title);
	if (ep->attr.href_base) e->attr.href_base = stracpy(ep->attr.href_base);
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
	e->name = e->options = NULL;
	e->namelen = 0;
	e->type = type;
	add_to_list(html_stack, e);
}

void
kill_html_stack_until(int ls, ...)
{
	int l;
	struct html_element *e = &html_top;

	if (ls) e = e->next;

	while ((void *)e != &html_stack) {
		int sk = 0;
		va_list arg;

		va_start(arg, ls);
		while (1) {
			unsigned char *s = va_arg(arg, unsigned char *);

			if (!s) break;
			if (!*s) {
				sk++;
			} else {
				int slen = strlen(s);

				if (!strlcasecmp(e->name, e->namelen, s, slen)) {
					if (!sk) {
						if (e->type < ELEMENT_KILLABLE) break;
						va_end(arg);
						goto killll;
					} else if (sk == 1) {
						va_end(arg);
						goto killl;
					} else {
						break;
					}
				}
			}
		}
		va_end(arg);

		if (e->type < ELEMENT_KILLABLE
		    || (!strlcasecmp(e->name, e->namelen, "TABLE", 5)))
			break;

		if (e->namelen == 2 && upcase(e->name[0]) == 'T') {
			unsigned char c = upcase(e->name[1]);

			if (c == 'D' || c == 'H' || c == 'R') break;
		}
		e = e->next;
	}
	return;

killl:
	e = e->prev;
killll:
	l = 0;
	while ((void *)e != &html_stack) {
		if (ls && e == html_stack.next) break;
		if (e->linebreak > l) l = e->linebreak;
		e = e->prev;
		kill_html_stack_item(e->next);
	}
	ln_break(l, line_break_f, ff);
}
