/* Terminal windows stuff. */
/* $Id: window.c,v 1.2 2003/05/04 20:06:51 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"


void
redraw_from_window(struct window *win)
{
	struct terminal *term = win->term;
	struct window *end = (void *)&term->windows;
	struct event ev = {EV_REDRAW, 0, 0, 0};

	ev.x = term->x;
	ev.y = term->y;
	if (term->redrawing) return;

	term->redrawing = 1;
	for (win = win->prev; win != end; win = win->prev) {
		IF_ACTIVE(win,term) win->handler(win, &ev, 0);
	}
	term->redrawing = 0;
}

void
redraw_below_window(struct window *win)
{
	int tr;
	struct terminal *term = win->term;
	struct window *end = win;
	struct event ev = {EV_REDRAW, 0, 0, 0};

	ev.x = term->x;
	ev.y = term->y;
	if (term->redrawing >= 2) return;
	tr = term->redrawing;
	win->term->redrawing = 2;
	for (win = term->windows.prev; win != end; win = win->prev) {
		IF_ACTIVE(win,term) win->handler(win, &ev, 0);
	}
	term->redrawing = tr;
}

static void
add_window_at_pos(struct terminal *term,
		  void (*handler)(struct window *, struct event *, int),
		  void *data, struct window *at)
{
	struct event ev = {EV_INIT, 0, 0, 0};
	struct window *win;

	ev.x = term->x;
	ev.y = term->y;

	win = mem_calloc(1, sizeof(struct window));
	if (!win) {
		if (data) mem_free(data);
		return;
	}

	win->handler = handler;
	win->data = data;
	win->term = term;
	win->type = WT_NORMAL;
	add_at_pos(at, win);
	win->handler(win, &ev, 0);
}

void
add_window(struct terminal *term,
	   void (*handler)(struct window *, struct event *, int),
	   void *data)
{
	add_window_at_pos(term, handler, data, (struct window *) &term->windows);
}

void
delete_window(struct window *win)
{
	struct event ev = {EV_ABORT, 0, 0, 0};

	win->handler(win, &ev, 1);
	del_from_list(win);
	if (win->data) mem_free(win->data);
	redraw_terminal(win->term);
	mem_free(win);
}

void
delete_window_ev(struct window *win, struct event *ev)
{
	struct window *w = win->next;

	if ((void *)w == &win->term->windows) w = NULL;
	delete_window(win);
	if (ev && w && w->next != w) w->handler(w, ev, 1);
}

void
get_parent_ptr(struct window *win, int *x, int *y)
{
	struct window *parent = win->next;

#if 0
	if ((void*) parent == &win->term->windows)
		parent = NULL;
	else
#endif
	if (parent->type)
                parent = get_tab_by_number(win->term, win->term->current_tab);

	if (parent) {
		*x = parent->xp;
		*y = parent->yp;
	} else {
		*x = 0;
		*y = 0;
	}
}


struct ewd {
	void (*fn)(void *);
	void *data;
	int b;
};

static void
empty_window_handler(struct window *win, struct event *ev, int fwd)
{
	struct window *n;
	struct ewd *ewd = win->data;
	int x, y;
	void (*fn)(void *) = ewd->fn;
	void *data = ewd->data;

	if (ewd->b) return;

	switch (ev->ev) {
		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
			get_parent_ptr(win, &x, &y);
			set_window_ptr(win, x, y);
			return;
		case EV_ABORT:
			fn(data);
			return;
		case EV_KBD:
		case EV_MOUSE:
			/* Silence compiler warnings */
			break;
	}

	ewd->b = 1;
	n = win->next;
	delete_window(win);
	fn(data);
	if (n->next != n) n->handler(n, ev, fwd);
}

void
add_empty_window(struct terminal *term, void (*fn)(void *), void *data)
{
	struct ewd *ewd = mem_alloc(sizeof(struct ewd));

	if (!ewd) return;
	ewd->fn = fn;
	ewd->data = data;
	ewd->b = 0;
	add_window(term, empty_window_handler, ewd);
}
