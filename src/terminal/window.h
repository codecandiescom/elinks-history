/* $Id: window.h,v 1.5 2003/11/05 10:46:53 zas Exp $ */

#ifndef EL__TERMINAL_WINDOW_H
#define EL__TERMINAL_WINDOW_H

#include "terminal/terminal.h"
#include "util/lists.h"

struct window {
	LIST_HEAD(struct window);

	void (*handler)(struct window *, struct term_event *, int fwd);
	void *data;
	struct terminal *term;
	int xpos, width;	/* Used for tabs focus detection. */
	int x, y;
	enum window_type {
		WT_NORMAL, /* normal window, ie. dialog one */
		WT_TAB, /* sorta sessions container */
	} type;
};

void redraw_from_window(struct window *);
void redraw_below_window(struct window *);
void add_window(struct terminal *, void (*)(struct window *, struct term_event *, int), void *);
void delete_window(struct window *);
void delete_window_ev(struct window *, struct term_event *ev);
#define set_window_ptr(window, x_, y_) do { (window)->x = (x_); (window)->y = (y_); } while (0)
void get_parent_ptr(struct window *, int *, int *);

void add_empty_window(struct terminal *, void (*)(void *), void *);

#endif
