/* Terminal interface - low-level displaying implementation. */
/* $Id: terminal.c,v 1.40 2003/07/28 08:51:13 jonas Exp $ */

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
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "lowlevel/signals.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/event.h"
#include "terminal/hardio.h"
#include "terminal/kbd.h"
#include "terminal/screen.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/textarea.h"


/* TODO: move this function elsewhere... -- Zas */
unsigned char *
get_cwd(void)
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

	foreachback (win, term->windows)
		if (!inactive_tab(win))
			win->handler(win, &ev, 0);

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
	alloc_screen(term, term->x, term->y);
	redraw_terminal_all(term);
}

void
cls_redraw_all_terminals(void)
{
	struct terminal *term;

	foreach (term, terminals)
		redraw_terminal_cls(term);
}

struct terminal *
init_term(int fdin, int fdout,
	  void (*root_window)(struct window *, struct event *, int))
{
	struct terminal_screen *screen;
	struct terminal *term = mem_calloc(1, sizeof(struct terminal));
	struct window *win;

	if (!term) {
		check_if_no_terminal();
		return NULL;
	}

	screen = mem_calloc(1, sizeof(struct terminal_screen));
	if (!screen) return NULL;

	term->screen = screen;
	screen->lcx = -1;
	screen->lcy = -1;

	term->fdin = fdin;
	term->fdout = fdout;
	term->master = (term->fdout == get_output_handle());
	term->dirty = 1;
	term->blocked = -1;
	term->spec = get_opt_rec(config_options, "terminal._template_");

	/* alloc_screen(term, 80, 25); */
	add_to_list(terminals, term);

	init_list(term->windows);

	win = init_tab(term, 1);
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
redraw_all_terminals(void)
{
	struct terminal *term;

	foreach (term, terminals)
		redraw_screen(term);
}

void
destroy_terminal(struct terminal *term)
{
	while ((term->windows.next) != &term->windows)
		delete_window(term->windows.next);

	/* if (term->cwd) mem_free(term->cwd); */
	if (term->title) mem_free(term->title);
	if (term->screen) done_screen(term->screen);

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
destroy_all_terminals(void)
{
	struct terminal *term;

	while ((void *) (term = terminals.next) != &terminals)
		destroy_terminal(term);
}

static void
check_if_no_terminal(void)
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
	set_handlers(term->fdin, (void (*)(void *)) in_term, NULL,
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

static int terminal_pipe[2];

int
check_terminal_pipes(void)
{
	return c_pipe(terminal_pipe);
}

void
close_terminal_pipes(void)
{
	close(terminal_pipe[0]);
	close(terminal_pipe[1]);
}

int
attach_terminal(int in, int out, int ctl, void *info, int len)
{
	struct terminal *term;

	if (set_nonblocking_fd(terminal_pipe[0]) < 0) return -1;
	if (set_nonblocking_fd(terminal_pipe[1]) < 0) return -1;
	handle_trm(in, out, out, terminal_pipe[1], ctl, info, len);

	mem_free(info);

	term = init_term(terminal_pipe[0], out, tabwin_func);
	if (!term) {
		close_terminal_pipes();
		return -1;
	}

	/* OK, this is race condition, but it must be so; GPM installs it's own
	 * buggy TSTP handler. */
	handle_basic_signals(term);

	return terminal_pipe[1];
}
