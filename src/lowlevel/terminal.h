/* $Id: terminal.h,v 1.23 2003/04/24 08:23:40 zas Exp $ */

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
};

extern struct list_head terminals;

extern unsigned char frame_dumb[];

int hard_write(int, unsigned char *, int);
int hard_read(int, unsigned char *, int);
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

/* Linux frame symbols table (it's magically converted to other terminals when
 * needed). */
/* In the screen image, they have attribute ATTR_FRAME; you should drop them
 * to the image using draw_frame_char(). */
/* TODO: When we'll support internal Unicode, this should be changed to some
 * Unicode sequences. --pasky */

enum frame_char {
	/* single-lined */
	FRAMES_ULCORNER = 218 | ATTR_FRAME,
	FRAMES_URCORNER = 191 | ATTR_FRAME,
	FRAMES_DLCORNER = 192 | ATTR_FRAME,
	FRAMES_DRCORNER = 217 | ATTR_FRAME,
	FRAMES_LTEE = 180 | ATTR_FRAME, /* => the tee points to the left => -| */
	FRAMES_RTEE = 195 | ATTR_FRAME,
	FRAMES_VLINE = 179 | ATTR_FRAME,
	FRAMES_HLINE = 196 | ATTR_FRAME,
	FRAMES_CROSS = 197 | ATTR_FRAME, /* + */

	/* double-lined */ /* TODO: The TEE-chars! */
	FRAMED_ULCORNER = 201 | ATTR_FRAME,
	FRAMED_URCORNER = 187 | ATTR_FRAME,
	FRAMED_DLCORNER = 200 | ATTR_FRAME,
	FRAMED_DRCORNER = 188 | ATTR_FRAME,
	FRAMED_VLINE = 186 | ATTR_FRAME,
	FRAMED_HLINE = 205 | ATTR_FRAME,
};

#define TERM_FN_TITLE	1
#define TERM_FN_RESIZE	2

void exec_on_terminal(struct terminal *, unsigned char *, unsigned char *, int);
void set_terminal_title(struct terminal *, unsigned char *);
void do_terminal_function(struct terminal *, unsigned char, unsigned char *);
void beep_terminal(struct terminal *);

#define term_send_event(terminal, event) ((struct window *) &(terminal)->windows)->next->handler((terminal)->windows.next, (event), 0)

#endif
