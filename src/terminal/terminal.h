/* $Id: terminal.h,v 1.7 2003/05/04 19:39:51 pasky Exp $ */

#ifndef EL__TERMINAL_TERMINAL_H
#define EL__TERMINAL_TERMINAL_H

#include "config/options.h"
#include "intl/charsets.h"
#include "util/lists.h"

typedef unsigned short chr;

enum event_type {
	EV_INIT,
	EV_KBD,
	EV_MOUSE,
	EV_REDRAW,
	EV_RESIZE,
	EV_ABORT,
};

struct event {
	enum event_type ev;
	long x;
	long y;
	long b;
};

#define MAX_TERM_LEN	32	/* this must be multiple of 8! (alignment problems) */
#define MAX_CWD_LEN	256	/* this must be multiple of 8! (alignment problems) */

enum term_mode_type {
	TERM_DUMB,
	TERM_VT100,
	TERM_LINUX,
	TERM_KOI8,
};

/* This is a bitmask describing the environment we are living in,
 * terminal-wise. We can then conditionally use various features available
 * in such an environment. */
enum term_env_type {
	/* This basically means that we can use the text i/o :). Always set. */
	ENV_CONSOLE = 1,
	/* We are running in a xterm-compatible box in some windowing
	 * environment. */
	ENV_XWIN = 2,
	/* We are running under a screen. */
	ENV_SCREEN = 4,
	/* We are running in a OS/2 VIO terminal. */
	ENV_OS2VIO = 8,
	/* BeOS text terminal. */
	ENV_BE = 16,
	/* We live in a TWIN text-mode windowing environment. */
	ENV_TWIN = 32,
	/* Microsoft Windows cmdline thing. */
	ENV_WIN32 = 64,
};

struct terminal {
	LIST_HEAD(struct terminal);

	int master;
	int fdin;
	int fdout;
	int x;
	int y;
	enum term_env_type environment;
	unsigned char term[MAX_TERM_LEN];
	unsigned char cwd[MAX_CWD_LEN];
	unsigned *screen;
	unsigned *last_screen;
	struct option *spec;
	int cx;
	int cy;
	int lcx;
	int lcy;
	int dirty;
	int redrawing;
	int blocked;
	unsigned char *input_queue;
	int qlen;
	int qfreespace;
	int current_tab;
	struct list_head windows;
	unsigned char *title;
	struct {
		unicode_val ucs;
		int len;
		int min;
	} utf_8;
};

struct window {
	LIST_HEAD(struct window);

	void (*handler)(struct window *, struct event *, int fwd);
	void *data;
	int xp, yp;
	struct terminal *term;
	enum window_type {
		WT_NORMAL,
		WT_ROOT, /* one per tab */
	} type;
};

extern struct list_head terminals;

extern unsigned char frame_dumb[];

unsigned char *get_cwd();
void set_cwd(unsigned char *);
struct terminal *init_term(int, int, void (*)(struct window *, struct event *, int));
void destroy_terminal(struct terminal *);
void redraw_terminal_ev(struct terminal *, int);
#define redraw_terminal(term) redraw_terminal_ev((term), EV_REDRAW)
#define redraw_terminal_all(term) redraw_terminal_ev((term), EV_RESIZE)
void redraw_terminal_cls(struct terminal *);
void cls_redraw_all_terminals();
void redraw_from_window(struct window *);
void redraw_below_window(struct window *);
void add_window(struct terminal *, void (*)(struct window *, struct event *, int), void *);
void delete_window(struct window *);
void delete_window_ev(struct window *, struct event *ev);
#define set_window_ptr(window, x, y) 	(window)->xp = (x), (window)->yp = (y)
void get_parent_ptr(struct window *, int *, int *);

struct window *init_tab(struct terminal *);
int number_of_tabs(struct terminal *term);
int get_tab_number(struct window *window);
struct window *get_tab_by_number(struct terminal *term, int num);
void switch_to_tab(struct terminal *term, int num);
void close_tab(struct terminal *term);
#define get_root_window(term) get_tab_by_number((term), (term)->current_tab)

void add_empty_window(struct terminal *, void (*)(void *), void *);
void redraw_all_terminals();
void destroy_all_terminals();
void block_itrm(int);
int unblock_itrm(int);
void exec_thread(unsigned char *, int);
void close_handle(void *);

#define TERM_FN_TITLE	1
#define TERM_FN_RESIZE	2

void exec_on_terminal(struct terminal *, unsigned char *, unsigned char *, int);
void set_terminal_title(struct terminal *, unsigned char *);
void do_terminal_function(struct terminal *, unsigned char, unsigned char *);

void term_send_event(struct terminal *, struct event *);

#endif
