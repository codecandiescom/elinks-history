/* Terminal windows stuff. */
/* $Id: window.c,v 1.15 2004/05/14 10:24:49 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "terminal/event.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"


void
redraw_from_window(struct window *win)
{
	struct terminal *term = win->term;
	struct term_event ev = INIT_TERM_EVENT(EV_REDRAW, term->width, term->height, 0);
	struct window *end = (void *) &term->windows;

	if (term->redrawing != 0) return;

	term->redrawing = 1;
	for (win = win->prev; win != end; win = win->prev) {
		if (!inactive_tab(win))
			win->handler(win, &ev, 0);
	}
	term->redrawing = 0;
}

void
redraw_below_window(struct window *win)
{
	struct terminal *term = win->term;
	struct term_event ev = INIT_TERM_EVENT(EV_REDRAW, term->width, term->height, 0);
	struct window *end = win;
	int tr = term->redrawing;

	if (term->redrawing > 1) return;

	term->redrawing = 2;
	for (win = term->windows.prev; win != end; win = win->prev) {
		if (!inactive_tab(win))
			win->handler(win, &ev, 0);
	}
	term->redrawing = tr;
}

static void
add_window_at_pos(struct terminal *term,
		  void (*handler)(struct window *, struct term_event *, int),
		  void *data, struct window *at)
{
	struct window *win = mem_calloc(1, sizeof(struct window));
	struct term_event ev = INIT_TERM_EVENT(EV_INIT, term->width, term->height, 0);

	if (!win) {
		mem_free_if(data);
		return;
	}

	win->handler = handler;
	win->data = data; /* freed later in delete_window() */
	win->term = term;
	win->type = WT_NORMAL;
	add_at_pos(at, win);
	win->handler(win, &ev, 0);
}

void
add_window(struct terminal *term,
	   void (*handler)(struct window *, struct term_event *, int),
	   void *data)
{
	add_window_at_pos(term, handler, data, (struct window *) &term->windows);
}

void
delete_window(struct window *win)
{
	struct term_event ev = INIT_TERM_EVENT(EV_ABORT, 0, 0, 0);

	/* Updating the status when destroying tabs needs this before the win
	 * handler call. */
	del_from_list(win);
	win->handler(win, &ev, 1);
	mem_free_if(win->data);
	redraw_terminal(win->term);
	mem_free(win);
}

void
delete_window_ev(struct window *win, struct term_event *ev)
{
	struct window *w = win->next;

	if ((void *) w == &win->term->windows) w = NULL;
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
		*x = parent->x;
		*y = parent->y;
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
empty_window_handler(struct window *win, struct term_event *ev, int fwd)
{
	struct terminal *term = win->term;
	struct ewd *ewd = win->data;
	void (*fn)(void *) = ewd->fn;
	void *data = ewd->data;

	if (ewd->b) return;

	switch (ev->ev) {
		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
			get_parent_ptr(win, &win->x, &win->y);
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
	delete_window(win);
	fn(data);
	term_send_event(term, ev);
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
