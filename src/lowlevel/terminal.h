/* $Id: terminal.h,v 1.8 2002/05/08 13:55:04 pasky Exp $ */

#ifndef EL__TERMINAL_H
#define EL__TERMINAL_H

#include "links.h" /* list_head */

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

#define MAX_TERM_LEN	16	/* this must be multiple of 8! (alignment problems) */

#define MAX_CWD_LEN	8192	/* this must be multiple of 8! (alignment problems) */

#define ATTR_FRAME	0x8000

enum term_mode_type {
	TERM_DUMB,
	TERM_VT100,
	TERM_LINUX,
	TERM_KOI8,
};

struct term_spec {
	struct term_spec *next;
	struct term_spec *prev;
	unsigned char term[MAX_TERM_LEN];
	enum term_mode_type mode;
	int m11_hack;
	int utf_8_io;
	int restrict_852;
	/* This means we always move cursor to (altx,alty) in set_cursor(),
	 * which is usually bottom right corner. */
	int block_cursor;
	int col;
	int charset;
};

enum term_env_type {
	ENV_XWIN = 1,
	ENV_SCREEN = 2,
	ENV_OS2VIO = 4,
	ENV_BE = 8,
	ENV_TWIN = 16,
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
	struct term_spec *spec;
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
		int ucs;
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

extern struct list_head term_specs;
extern struct list_head terminals;

extern unsigned char frame_dumb[];

int hard_write(int, unsigned char *, int);
int hard_read(int, unsigned char *, int);
unsigned char *get_cwd();
void set_cwd(unsigned char *);
struct terminal *init_term(int, int, void (*)(struct window *, struct event *, int));
void sync_term_specs();
struct term_spec *new_term_spec(unsigned char *);
void free_term_specs();
void destroy_terminal(struct terminal *);
void redraw_terminal(struct terminal *);
void redraw_terminal_all(struct terminal *);
void redraw_terminal_cls(struct terminal *);
void cls_redraw_all_terminals();
void redraw_from_window(struct window *);
void redraw_below_window(struct window *);
void add_window(struct terminal *, void (*)(struct window *, struct event *, int), void *);
void add_window_at_pos(struct terminal *, void (*)(struct window *, struct event *, int), void *, struct window *);
void delete_window(struct window *);
void delete_window_ev(struct window *, struct event *ev);
void set_window_ptr(struct window *, int, int);
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

#endif
