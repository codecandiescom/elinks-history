/* $Id: terminal.h,v 1.22 2003/07/28 08:51:13 jonas Exp $ */

#ifndef EL__TERMINAL_TERMINAL_H
#define EL__TERMINAL_TERMINAL_H

#include "config/options.h"
#include "intl/charsets.h"
#include "terminal/event.h"
#include "util/lists.h"


/* Some constants for the strings inside of {struct terminal}. */

#define MAX_TERM_LEN	32	/* this must be multiple of 8! (alignment problems) */
#define MAX_CWD_LEN	256	/* this must be multiple of 8! (alignment problems) */

/* The terminal type, meaningful for frames (lines) drawing. */
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

/* Lives in terminal/window.h and terminal/screen.h . */
struct window;
struct terminal_screen;

/* This is one of the axis of ELinks' user interaction. {struct terminal}
 * defines the terminal ELinks is running on --- each ELinks instance has
 * one. It contains the basic terminal attributes, the settings associated
 * with this terminal, screen content (and more abstract description of what
 * is currently displayed on it) etc. It also maintains some runtime
 * information about the actual ELinks instance owning this terminal. */
/* TODO: Regroup the following into logical chunks. --pasky */
struct terminal {
	LIST_HEAD(struct terminal); /* {terminals} */

	/* This is (at least partially) a stack of all the windows living in
	 * this terminal. A window can be wide range of stuff, from a menu box
	 * through classical dialog window to a tab. See terminal/window.h for
	 * more on windows.
	 *
	 * Tabs are special windows, though, and you never want to display them
	 * all, but only one of them. ALWAYS check {inactive_tab} during
	 * iterations through this list (unless it is really useless or you
	 * are sure what are you doing) to make sure that you don't distribute
	 * events etc to inactive tabs.
	 *
	 * The stack is top-down, thus .next is the stack's top, the current
	 * window. .prev is the first tab.
	 *
	 * FIXME: Tabs violate the stack nature of this list, they appear there
	 * randomly but always in the order in which they were inserted there.
	 * Eventually, they should all live at the stack bottom, with the
	 * actual tab living on the VERY bottom. --pasky */
	struct list_head windows; /* {struct window} */

	/* The specification of terminal in terms of terminal options. */
	struct option *spec;

	/* This is the terminal's current title, as perhaps displayed somewhere
	 * in the X window frame or so. */
	unsigned char *title;

	/* This is the queue of events as coming from the other ELinks instance
	 * owning this terminal. */
	unsigned char *input_queue;

	/* This is the screen. See terminal/screen.h */
	struct terminal_screen *screen;

	/* Indicates the master terminal, that is the terminal under
	 * supervision of the master ELinks instance (the one doing all the
	 * work and even maintaining these structures ;-). */
	int master;

	/* These are pipes for communication with the ELinks instance owning
	 * this terminal. */
	int fdin, fdout;

	int x, y;

	/* We are sure that @screen and the physical screen are out of sync. */
	int dirty;

	/* Indicates whether we are currently in the process of redrawing the
	 * stuff being displayed on the terminal. It is typically used to
	 * prevent redrawing inside of redrawing. */
	int redrawing;

	/* This indicates that the terminal is blocked, that is nothing should
	 * be drawn on it etc. Typically an external program is running on it
	 * right now. */
	int blocked;

	int qlen;
	int qfreespace;

	/* The current tab number. */
	int current_tab;

	/* The type of environment this terminal lives in. */
	enum term_env_type environment;

	/* $TERM env. variable value */
	unsigned char term[MAX_TERM_LEN];

	/* The current working directory for this terminal / ELinks instance. */
	unsigned char cwd[MAX_CWD_LEN];

	/* Something weird regarding the UTF8 I/O. */
	struct {
		unicode_val ucs;
		int len;
		int min;
	} utf_8;
};

/* We keep track about all the terminals in this list. */
extern struct list_head terminals;


extern unsigned char frame_dumb[];

unsigned char *get_cwd(void);
void set_cwd(unsigned char *);
struct terminal *init_term(int, int, void (*)(struct window *, struct event *, int));
void destroy_terminal(struct terminal *);
void redraw_terminal_ev(struct terminal *, int);
#define redraw_terminal(term) redraw_terminal_ev((term), EV_REDRAW)
#define redraw_terminal_all(term) redraw_terminal_ev((term), EV_RESIZE)
void redraw_terminal_cls(struct terminal *);
void cls_redraw_all_terminals(void);

void redraw_all_terminals(void);
void destroy_all_terminals(void);
void block_itrm(int);
int unblock_itrm(int);
void exec_thread(unsigned char *, int);
void close_handle(void *);

#define TERM_FN_TITLE	1
#define TERM_FN_RESIZE	2

void exec_on_terminal(struct terminal *, unsigned char *, unsigned char *, int);
void set_terminal_title(struct terminal *, unsigned char *);
void do_terminal_function(struct terminal *, unsigned char, unsigned char *);

int check_terminal_pipes(void);
void close_terminal_pipes(void);
int attach_terminal(int in, int out, int ctl, void *info, int len);

#endif /* EL__TERMINAL_TERMINAL_H */
