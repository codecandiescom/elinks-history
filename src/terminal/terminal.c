/* Terminal interface - low-level displaying implementation. */
/* $Id: terminal.c,v 1.69 2004/06/13 13:25:50 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bookmarks/bookmarks.h"
#include "main.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "lowlevel/signals.h"
#include "osdep/osdep.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/event.h"
#include "terminal/hardio.h"
#include "terminal/kbd.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"
#include "viewer/text/textarea.h"


INIT_LIST_HEAD(terminals);


static void check_if_no_terminal(void);


void
redraw_terminal_ev(struct terminal *term, int e)
{
	struct term_event ev = INIT_TERM_EVENT(e, term->width, term->height, 0);

	term_send_event(term, &ev);
}

void
cls_redraw_all_terminals(void)
{
	struct terminal *term;

	foreach (term, terminals)
		redraw_terminal_cls(term);
}

struct terminal *
init_term(int fdin, int fdout)
{
	struct terminal *term = mem_calloc(1, sizeof(struct terminal));

	if (!term) {
		check_if_no_terminal();
		return NULL;
	}

	term->screen = init_screen();
	if (!term->screen) {
		mem_free(term);
		return NULL;
	}

	init_list(term->windows);

	term->fdin = fdin;
	term->fdout = fdout;
	term->master = (term->fdout == get_output_handle());
	term->blocked = -1;
	term->spec = get_opt_rec(config_options, "terminal._template_");
	object_lock(term->spec);

	add_to_list(terminals, term);

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
#ifdef CONFIG_BOOKMARKS
	if (get_opt_bool("ui.sessions.auto_save")
	    && !get_opt_bool_tree(cmdline_options, "anonymous")) {
		bookmark_terminal_tabs(term,
			get_opt_str("ui.sessions.auto_save_foldername"));
	}
#endif

	while (!list_empty(term->windows))
		delete_window(term->windows.next);

	/* mem_free_if(term->cwd); */
	mem_free_if(term->title);
	if (term->screen) done_screen(term->screen);

	set_handlers(term->fdin, NULL, NULL, NULL, NULL);
	mem_free_if(term->input_queue);

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

	object_unlock(term->spec);
	mem_free(term);
	check_if_no_terminal();
}

void
destroy_all_terminals(void)
{
	while (!list_empty(terminals))
		destroy_terminal(terminals.next);
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
exec_shell(struct terminal *term)
{
	unsigned char *sh;

	if (!can_open_os_shell(term->environment)) return;

	sh = GETSHELL;
	if (!sh || !*sh) sh = DEFAULT_SHELL;
	if (sh && *sh)
		exec_on_terminal(term, sh, "", 1);
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
	mem_free_set(&term->title, stracpy(title));
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

struct terminal *
attach_terminal(int in, int out, int ctl, void *info, int len)
{
	struct terminal *term;

	if (set_nonblocking_fd(terminal_pipe[0]) < 0) return NULL;
	if (set_nonblocking_fd(terminal_pipe[1]) < 0) return NULL;
	handle_trm(in, out, out, terminal_pipe[1], ctl, info, len);

	term = init_term(terminal_pipe[0], out);
	if (!term) {
		close_terminal_pipes();
		return NULL;
	}

	return term;
}
