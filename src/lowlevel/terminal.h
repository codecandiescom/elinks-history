/* $Id: terminal.h,v 1.17 2002/09/11 15:33:32 zas Exp $ */

#ifndef EL__LOWLEVEL_TERMINAL_H
#define EL__LOWLEVEL_TERMINAL_H

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

#define ATTR_FRAME	0x8000

enum term_mode_type {
	TERM_DUMB,
	TERM_VT100,
	TERM_LINUX,
	TERM_KOI8,
};

enum term_env_type {
	ENV_XWIN = 1,
	ENV_SCREEN = 2,
	ENV_OS2VIO = 4,
	ENV_BE = 8,
	ENV_TWIN = 16,
	ENV_WIN32 = 32,
};

struct terminal {
	struct terminal *next;
	struct terminal *prev;
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
	struct list_head windows;
	unsigned char *title;
	struct {
		unicode_val ucs;
		int len;
		int min;
	} utf_8;
};

struct window {
	struct window *next;
	struct window *prev;
	void (*handler)(struct window *, struct event *, int fwd);
	void *data;
	int xp, yp;
	struct terminal *term;
};

extern struct list_head terminals;

extern unsigned char frame_dumb[];

int hard_write(int, unsigned char *, int);
int hard_read(int, unsigned char *, int);
unsigned char *get_cwd();
void set_cwd(unsigned char *);
struct terminal *init_term(int, int, void (*)(struct window *, struct event *, int));
void destroy_terminal(struct terminal *);
#define redraw_terminal(term) redraw_terminal_ev((term), EV_REDRAW)
#define redraw_terminal_all(term) redraw_terminal_ev((term), EV_RESIZE)
void redraw_terminal_cls(struct terminal *);
void cls_redraw_all_terminals();
void redraw_from_window(struct window *);
void redraw_below_window(struct window *);
void add_window(struct terminal *, void (*)(struct window *, struct event *, int), void *);
void add_window_at_pos(struct terminal *, void (*)(struct window *, struct event *, int), void *, struct window *);
void delete_window(struct window *);
void delete_window_ev(struct window *, struct event *ev);
#define set_window_ptr(window, x, y) 	(window)->xp = (x), (window)->yp = (y)
void get_parent_ptr(struct window *, int *, int *);
struct window *get_root_window(struct terminal *);
void add_empty_window(struct terminal *, void (*)(void *), void *);
void redraw_screen(struct terminal *);
void redraw_all_terminals();
void set_char(struct terminal *, int, int, unsigned);
unsigned get_char(struct terminal *, int, int);
void set_color(struct terminal *, int, int, unsigned);
void set_only_char(struct terminal *, int, int, unsigned);
void set_line(struct terminal *, int, int, int, chr *);
void set_line_color(struct terminal *, int, int, int, unsigned);
void fill_area(struct terminal *, int, int, int, int, unsigned);
void draw_frame(struct terminal *, int, int, int, int, unsigned, int);
void print_text(struct terminal *, int, int, int, unsigned char *, unsigned);
void set_cursor(struct terminal *, int, int, int, int);
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

#define term_send_event(terminal, event) ((struct window *) &(terminal)->windows)->next->handler((terminal)->windows.next, (event), 0)

#endif
