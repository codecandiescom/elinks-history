/* Terminal interface - low-level displaying implementation. */
/* $Id: terminal.c,v 1.20 2003/05/08 21:50:08 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "main.h"
#include "dialogs/menu.h" /* XXX */
#include "lowlevel/select.h"
#include "lowlevel/timer.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/hardio.h"
#include "terminal/kbd.h"
#include "terminal/screen.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/view.h"

/* TODO: move this function elsewhere... -- Zas */
unsigned char *
get_cwd()
{
	int bufsize = 128;
	unsigned char *buf;

	while (1) {
		buf = mem_alloc(bufsize);
		if (!buf) return NULL;
		if (getcwd(buf, bufsize)) return buf;
		mem_free(buf);

		if (errno == EINTR) continue;
		if (errno != ERANGE) return NULL;
		bufsize += 128;
	}

	return NULL;
}

/* TODO: move this function elsewhere... -- Zas */
void
set_cwd(unsigned char *path)
{
	if (path) while (chdir(path) && errno == EINTR);
}


INIT_LIST_HEAD(terminals);

static void
alloc_term_screen(struct terminal *term, int x, int y)
{
	unsigned *s;
	unsigned *t;
	int space = x * y * sizeof(unsigned);

	s = mem_realloc(term->screen, space);
	if (!s) return;

	t = mem_realloc(term->last_screen, space);
	if (!t) {
		mem_free(s);
		return;
	}

	memset(t, -1, space);
	term->x = x;
	term->y = y;
	term->last_screen = t;
	memset(s, 0, space);
	term->screen = s;
	term->dirty = 1;
}


static void in_term(struct terminal *);
static void check_if_no_terminal(void);


void
redraw_terminal_ev(struct terminal *term, int e)
{
	struct window *win;
	struct event ev = {0, 0, 0, 0};

	ev.ev = e;
	ev.x = term->x;
	ev.y = term->y;
	clear_terminal(term);
	term->redrawing = 2;

	foreachback(win, term->windows)
		IF_ACTIVE(win,term) win->handler(win, &ev, 0);

	term->redrawing = 0;
}

#if 0
/* These are macros - see terminal.h */
void
redraw_terminal(struct terminal *term)
{
	redraw_terminal_ev(term, EV_REDRAW);
}

void
redraw_terminal_all(struct terminal *term)
{
	redraw_terminal_ev(term, EV_RESIZE);
}
#endif

void
redraw_terminal_cls(struct terminal *term)
{
	erase_screen(term);
	alloc_term_screen(term, term->x, term->y);
	redraw_terminal_all(term);
}

void
cls_redraw_all_terminals()
{
	struct terminal *term;

	foreach (term, terminals)
		redraw_terminal_cls(term);
}

struct terminal *
init_term(int fdin, int fdout,
	  void (*root_window)(struct window *, struct event *, int))
{
	struct window *win;
	struct terminal *term = mem_calloc(1, sizeof(struct terminal));

	if (!term) {
		check_if_no_terminal();
		return NULL;
	}

	term->fdin = fdin;
	term->fdout = fdout;
	term->master = (term->fdout == get_output_handle());
	term->lcx = -1;
	term->lcy = -1;
	term->dirty = 1;
	term->blocked = -1;
	term->spec = get_opt_rec(&root_options, "terminal._template_");

	/* alloc_term_screen(term, 80, 25); */
	add_to_list(terminals, term);

	init_list(term->windows);

	win = init_tab(term);
	if (!win) {
		del_from_list(term);
		mem_free(term);
		check_if_no_terminal();
		return NULL;
	}
	win->handler = root_window;

	set_handlers(fdin, (void (*)(void *)) in_term, NULL,
		     (void (*)(void *)) destroy_terminal, term);
	return term;
}

void
term_send_event(struct terminal *term, struct event *ev)
{
	struct window *first_win = term->windows.next;
	struct window *win;

	/* We need to send event to correct tab, not to the first one. --karpov */
	/* ...if we want to send it to a tab at all. --pasky */

	win = first_win->type == WT_TAB ? get_current_tab(term) : first_win;
	if (!win)
		internal("No tab to send the event to!");

	win->handler(win, ev, 0);
}

static void
term_send_ucs(struct terminal *term, struct event *ev, unicode_val u)
{
	unsigned char *recoded;

	if (u == 0xA0) u = ' ';
	recoded = u2cp(u, get_opt_int_tree(term->spec, "charset"));
	if (!recoded) recoded = "*";
	while (*recoded) {
		ev->x = *recoded;
		term_send_event(term, ev);
		recoded++;
	}
}

static void
in_term(struct terminal *term)
{
	struct event *ev;
	int r;
	unsigned char *iq = term->input_queue;

	if (!iq || !term->qfreespace || term->qfreespace - term->qlen > ALLOC_GR) {
		int newsize = ((term->qlen + ALLOC_GR) & ~(ALLOC_GR - 1));

		iq = mem_realloc(term->input_queue, newsize);
		if (!iq) {
			destroy_terminal(term);
			return;
		}
		term->input_queue = iq;
		term->qfreespace = newsize - term->qlen;
	}

	r = read(term->fdin, iq + term->qlen, term->qfreespace);
	if (r <= 0) {
		if (r == -1 && errno != ECONNRESET)
			error("ERROR: error %d on terminal: could not read event", errno);
		destroy_terminal(term);
		return;
	}
	term->qlen += r;
	term->qfreespace -= r;

test_queue:
	if (term->qlen < sizeof(struct event)) return;
	ev = (struct event *)iq;
	r = sizeof(struct event);

	if (ev->ev != EV_INIT
	    && ev->ev != EV_RESIZE
	    && ev->ev != EV_REDRAW
	    && ev->ev != EV_KBD
	    && ev->ev != EV_MOUSE
	    && ev->ev != EV_ABORT) {
		error("ERROR: error on terminal: bad event %d", ev->ev);
		goto mm;
	}

	if (ev->ev == EV_INIT) {
		int init_len;
		int evterm_len = sizeof(struct event) + MAX_TERM_LEN;
		int evtermcwd_len = evterm_len + MAX_CWD_LEN;
		int evtermcwd1int_len = evtermcwd_len + sizeof(int);
		int evtermcwd2int_len = evtermcwd1int_len + sizeof(int);

		if (term->qlen < evtermcwd2int_len) return;
		init_len = *(int *)(iq + evtermcwd1int_len);

		if (term->qlen < evtermcwd2int_len + init_len) return;

		memcpy(term->term, iq + sizeof(struct event), MAX_TERM_LEN);
		term->term[MAX_TERM_LEN - 1] = 0;

		{
			unsigned char name[MAX_TERM_LEN + 10];
			int i = 0, badchar = 0;

			strcpy(name, "terminal.");

			/* We check TERM env. var for sanity, and fallback to
			 * _template_ if needed. This way we prevent
			 * elinks.conf potential corruption. */
			while (term->term[i]) {
				if (!isA(term->term[i])) {
					badchar = 1;
					break;
				}
				i++;
			}

			if (badchar) {
				error("WARNING: terminal name contains illicit chars.\n");
				strcat(name, "_template_");
			} else {
				strcat(name, term->term);
			}

			term->spec = get_opt_rec(&root_options, name);
		}

		memcpy(term->cwd, iq + evterm_len, MAX_CWD_LEN);
		term->cwd[MAX_CWD_LEN - 1] = 0;

		term->environment = *(int *)(iq + evtermcwd_len);
		ev->b = (long) decode_session_info(iq + evtermcwd1int_len, evtermcwd2int_len);
		r = evtermcwd2int_len + init_len;
	}

	if (ev->ev == EV_REDRAW || ev->ev == EV_RESIZE || ev->ev == EV_INIT) {
		struct window *win;

send_redraw:
		if (ev->x < 0 || ev->y < 0) {
			error("ERROR: bad terminal size: %d, %d",
			      (int) ev->x, (int) ev->y);
			goto mm;
		}

		alloc_term_screen(term, ev->x, ev->y);
		clear_terminal(term);
		erase_screen(term);
		term->redrawing = 1;
		foreachback(win, term->windows) {
			/* Note that you do NOT want to ever go and create new
			 * window inside EV_INIT handler (it'll get second
			 * EV_INIT here). Work out some hack, like me ;-).
			 * --pasky */
			IF_ACTIVE(win,term) win->handler(win, ev, 0);
		}
		{
			extern int startup_goto_dialog_paint;
			extern struct session *startup_goto_dialog_ses;

			if (startup_goto_dialog_paint) {
				dialog_goto_url(startup_goto_dialog_ses, "");
				startup_goto_dialog_paint = 0;
			}
		}
		term->redrawing = 0;
	}

	if (ev->ev == EV_KBD || ev->ev == EV_MOUSE) {
		reset_timer();
		if (ev->ev == EV_KBD && upcase(ev->x) == 'L'
		    && ev->y == KBD_CTRL) {
			ev->ev = EV_REDRAW;
			ev->x = term->x;
			ev->y = term->y;
			goto send_redraw;
		}
		else if (ev->ev == EV_KBD && ev->x == KBD_CTRL_C)
			((struct window *) &term->windows)->prev->handler(term->windows.prev, ev, 0);
		else if (ev->ev == EV_KBD) {
			if (term->utf_8.len) {
				if ((ev->x & 0xC0) == 0x80
				    && get_opt_bool_tree(term->spec, "utf_8_io")) {
					term->utf_8.ucs <<= 6;
					term->utf_8.ucs |= ev->x & 0x3F;
					if (! --term->utf_8.len) {
						unicode_val u = term->utf_8.ucs;

						if (u < term->utf_8.min) u = UCS_NO_CHAR;
						term_send_ucs(term, ev, u);
					}
					goto mm;
				} else {
					term->utf_8.len = 0;
					term_send_ucs(term, ev, UCS_NO_CHAR);
				}
			}
			if (ev->x < 0x80 || ev->x > 0xFF
			    || !get_opt_bool_tree(term->spec, "utf_8_io")) {
				term_send_event(term, ev);
				goto mm;
			} else if ((ev->x & 0xC0) == 0xC0 && (ev->x & 0xFE) != 0xFE) {
				int mask, len = 0, cov = 0x80;

				for (mask = 0x80; ev->x & mask; mask >>= 1) {
					len++;
					term->utf_8.min = cov;
					cov = 1 << (1 + 5 * len);
				}
				term->utf_8.len = len - 1;
				term->utf_8.ucs = ev->x & (mask - 1);
				goto mm;
			}
			term_send_ucs(term, ev, UCS_NO_CHAR);
		} else term_send_event(term, ev);
	}

	if (ev->ev == EV_ABORT) destroy_terminal(term);
	/* redraw_screen(term); */
mm:
	if (term->qlen == r) {
		term->qlen = 0;
	} else {
		term->qlen -= r;
		memmove(iq, iq + r, term->qlen);
	}
	term->qfreespace += r;

	goto test_queue;
}

void
redraw_all_terminals()
{
	struct terminal *term;

	foreach(term, terminals)
		redraw_screen(term);
}

void
destroy_terminal(struct terminal *term)
{
	while ((term->windows.next) != &term->windows)
		delete_window(term->windows.next);

	/* if (term->cwd) mem_free(term->cwd); */
	if (term->title) mem_free(term->title);
	if (term->screen) mem_free(term->screen);
	if (term->last_screen) mem_free(term->last_screen);

	set_handlers(term->fdin, NULL, NULL, NULL, NULL);
	if (term->input_queue) mem_free(term->input_queue);

	if (term->blocked != -1) {
		close(term->blocked);
		set_handlers(term->blocked, NULL, NULL, NULL, NULL);
	}

	del_from_list(term);
	close(term->fdin);

	if (term->fdout != 1) {
		if (term->fdout != term->fdin) close(term->fdout);
	} else {
		unhandle_terminal_signals(term);
		free_all_itrms();
#ifndef NO_FORK_ON_EXIT
		if (!list_empty(terminals)) {
			if (fork()) exit(0);
		}
#endif
	}

	mem_free(term);
	check_if_no_terminal();
}

void
destroy_all_terminals()
{
	struct terminal *term;

	while ((void *) (term = terminals.next) != &terminals)
		destroy_terminal(term);
}

static void
check_if_no_terminal()
{
	terminate = list_empty(terminals);
}

void
exec_thread(unsigned char *path, int p)
{
	int plen = strlen(path + 1) + 2;

#if defined(HAVE_SETPGID) && !defined(BEOS) && !defined(HAVE_BEGINTHREAD)
	if (path[0] == 2) setpgid(0, 0);
#endif
	exe(path + 1);
	close(p);
	if (path[plen]) unlink(path + plen);
}

void
close_handle(void *h)
{
	close((int) h);
	set_handlers((int) h, NULL, NULL, NULL, NULL);
}

static void
unblock_terminal(struct terminal *term)
{
	close_handle((void *)term->blocked);
	term->blocked = -1;
	set_handlers(term->fdin, (void (*)(void *))in_term, NULL,
		     (void (*)(void *))destroy_terminal, term);
	unblock_itrm(term->fdin);
	redraw_terminal_cls(term);
	if (textarea_editor)	/* XXX */
		textarea_edit(1, NULL, NULL, NULL, NULL, NULL);
}

void
exec_on_terminal(struct terminal *term, unsigned char *path,
		 unsigned char *delete, int fg)
{
	int plen;
	int dlen = strlen(delete);

	if (path && !*path) return;
	if (!path) {
		path = "";
		plen = 0;
	} else {
		plen = strlen(path);
	}

#ifdef NO_FG_EXEC
	fg = 0;
#endif
	if (term->master) {
		if (!*path) dispatch_special(delete);
		else {
			int blockh;
			unsigned char *param;
			int param_size;

			if (is_blocked() && fg) {
				unlink(delete);
				return;
			}

			param_size = plen + dlen + 2 /* 2 null char */ + 1 /* fg */;
			param = mem_alloc(param_size);
			if (!param) return;

			param[0] = fg;
			memcpy(param + 1, path, plen + 1);
			memcpy(param + 1 + plen + 1, delete, dlen + 1);

			if (fg == 1) block_itrm(term->fdin);

			blockh = start_thread((void (*)(void *, int))exec_thread,
					      param, param_size);
			if (blockh == -1) {
				if (fg == 1) unblock_itrm(term->fdin);
				mem_free(param);
				return;
			}

			mem_free(param);
			if (fg == 1) {
				term->blocked = blockh;
				set_handlers(blockh,
					     (void (*)(void *)) unblock_terminal,
					     NULL,
					     (void (*)(void *)) unblock_terminal,
					     term);
				set_handlers(term->fdin, NULL, NULL,
					     (void (*)(void *)) destroy_terminal,
					     term);
				/* block_itrm(term->fdin); */
			} else {
				set_handlers(blockh, close_handle, NULL,
					     close_handle, (void *) blockh);
			}
		}
	} else {
		int data_size = plen + dlen + 1 /* 0 */ + 1 /* fg */ + 2 /* 2 null char */;
		unsigned char *data = mem_alloc(data_size);

		if (data) {
			data[0] = 0;
			data[1] = fg;
			memcpy(data + 2, path, plen + 1);
			memcpy(data + 2 + plen + 1, delete, dlen + 1);
			hard_write(term->fdout, data, data_size);
			mem_free(data);
		}
#if 0
		char x = 0;
		hard_write(term->fdout, &x, 1);
		x = fg;
		hard_write(term->fdout, &x, 1);
		hard_write(term->fdout, path, strlen(path) + 1);
		hard_write(term->fdout, delete, strlen(delete) + 1);
#endif
	}
}

void
do_terminal_function(struct terminal *term, unsigned char code,
		     unsigned char *data)
{
	int data_len = strlen(data);
	unsigned char *x_data = fmem_alloc(data_len + 1 /* code */ + 1 /* null char */);

	if (!x_data) return;
	x_data[0] = code;
	memcpy(x_data + 1, data, data_len + 1);
	exec_on_terminal(term, NULL, x_data, 0);
	fmem_free(x_data);
}

void
set_terminal_title(struct terminal *term, unsigned char *title)
{
	if (term->title && !strcmp(title, term->title)) return;
	if (term->title) mem_free(term->title);
	term->title = stracpy(title);
	do_terminal_function(term, TERM_FN_TITLE, title);
}
