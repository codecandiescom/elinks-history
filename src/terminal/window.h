/* $Id: window.h,v 1.3 2003/09/18 13:07:11 zas Exp $ */

#ifndef EL__TERMINAL_WINDOW_H
#define EL__TERMINAL_WINDOW_H

#include "terminal/terminal.h"
#include "util/lists.h"

struct window {
	LIST_HEAD(struct window);

	void (*handler)(struct window *, struct event *, int fwd);
	void *data;
	struct terminal *term;
	int xpos, width;	/* Used for tabs focus detection. */
	int xp, yp;
	enum window_type {
		WT_NORMAL, /* normal window, ie. dialog one */
		WT_TAB, /* sorta sessions container */
	} type;
};

void redraw_from_window(struct window *);
void redraw_below_window(struct window *);
void add_window(struct terminal *, void (*)(struct window *, struct event *, int), void *);
void delete_window(struct window *);
void delete_window_ev(struct window *, struct event *ev);
#define set_window_ptr(window, x, y) (window)->xp = (x), (window)->yp = (y)
void get_parent_ptr(struct window *, int *, int *);

void add_empty_window(struct terminal *, void (*)(void *), void *);

#endif
