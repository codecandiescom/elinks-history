/* $Id: window.h,v 1.7 2003/12/02 19:07:52 jonas Exp $ */

#ifndef EL__TERMINAL_WINDOW_H
#define EL__TERMINAL_WINDOW_H

#include "util/lists.h"

struct term_event;
struct terminal;

enum window_type {
	/* Normal windows: */
	/* Used for things like dialogs. The default type when adding windows
	 * with add_window(). */
	WT_NORMAL,

	/* Tab windows: */
	/* Tabs are a separate session and has separate history, current
	 * document and action-in-progress .. basically a separate browsing
	 * state. */
	WT_TAB,
};

struct window {
	LIST_HEAD(struct window);

	enum window_type type;

	/* The window event handler */
	void (*handler)(struct window *, struct term_event *, int fwd);

	/* For tab windows the session is stored in @data. For normal windows
	 * it can contain dialog data. */
	/* It is free()'d by delete_window() */
	void *data;

	/* The terminal (and screen) that hosts the window */
	struct terminal *term;

	/* Used for tabs focus detection. */
	int xpos, width;
	int x, y;
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
