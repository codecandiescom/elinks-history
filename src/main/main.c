/* The main program - startup */
/* $Id: main.c,v 1.11 2002/03/30 21:52:28 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LUA
#include <lua.h>
#include <lualib.h>
#endif
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <links.h>

#include <main.h>
#include <bookmarks/bookmarks.h>
#include <config/default.h>
#include <config/kbdbind.h>
#include <cookies/cookies.h>
#include <dialogs/menu.h>
#include <document/cache.h>
#include <document/download.h>
#include <document/dump.h>
#include <document/session.h>
#include <document/html/colors.h>
#include <document/html/renderer.h>
#include <intl/charsets.h>
#include <intl/language.h>
#include <lowlevel/af_unix.h>
#include <lowlevel/dns.h>
#include <lowlevel/kbd.h>
#include <lowlevel/sched.h>
#include <lowlevel/select.h>
#include <lowlevel/terminal.h>
#include <lua/lua.h>
#include <protocol/types.h>
#include <ssl/ssl.h>
#include <util/error.h>

enum retval retval = RET_OK;

void unhandle_basic_signals(struct terminal *);


void
sig_terminate(struct terminal *t)
{
	unhandle_basic_signals(t);
	terminate = 1;
	retval = RET_SIGNAL;
}

void
sig_intr(struct terminal *t)
{
	if (!t) {
		unhandle_basic_signals(t);
		terminate = 1;
	} else {
		unhandle_basic_signals(t);
		exit_prog(t, NULL, NULL);
	}
}

void
sig_ctrl_c(struct terminal *t)
{
	if (!is_blocked()) kbd_ctrl_c();
}

void
sig_ign(void *x)
{
}

void
sig_tstp(struct terminal *t)
{
#ifdef SIGSTOP
	int pid = getpid();

	block_itrm(0);
#if defined (SIGCONT) && defined(SIGTTOU)
	if (!fork()) {
		sleep(1);
		kill(pid, SIGCONT);
		exit(0);
	}
#endif
	raise(SIGSTOP);
#endif
}

void
sig_cont(struct terminal *t)
{
	if (!unblock_itrm(0)) {
		/* redraw_terminal_cls(t); */
		resize_terminal();
	} /* else {
		register_bottom_half(raise, SIGSTOP);
	} */
}


void
handle_basic_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, (void (*)(void *))sig_intr, term, 0);
	install_signal_handler(SIGINT, (void (*)(void *))sig_ctrl_c, term, 0);
	install_signal_handler(SIGTERM, (void (*)(void *))sig_terminate, term, 0);
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, (void (*)(void *))sig_tstp, term, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, (void (*)(void *))sig_tstp, term, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, (void (*)(void *))sig_ign, term, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, (void (*)(void *))sig_cont, term, 0);
#endif
}

#if 0
void handle_slave_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, (void (*)(void *))sig_terminate, term, 0);
	install_signal_handler(SIGINT, (void (*)(void *))sig_terminate, term, 0);
	install_signal_handler(SIGTERM, (void (*)(void *))sig_terminate, term, 0);
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, (void (*)(void *))sig_tstp, term, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, (void (*)(void *))sig_tstp, term, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, (void (*)(void *))sig_ign, term, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, (void (*)(void *))sig_cont, term, 0);
#endif
}
#endif


void
unhandle_terminal_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, NULL, NULL, 0);
	install_signal_handler(SIGINT, NULL, NULL, 0);
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, NULL, NULL, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, NULL, NULL, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, NULL, NULL, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, NULL, NULL, 0);
#endif
}

void
unhandle_basic_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, NULL, NULL, 0);
	install_signal_handler(SIGINT, NULL, NULL, 0);
	install_signal_handler(SIGTERM, NULL, NULL, 0);
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, NULL, NULL, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, NULL, NULL, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, NULL, NULL, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, NULL, NULL, 0);
#endif
}


int
attach_terminal(int in, int out, int ctl, void *info, int len)
{
	struct terminal *term;
	int fd[2];
	
	if (c_pipe(fd)) {
		mem_free(info);
		error("ERROR: can't create pipe for internal communication");
		return -1;
	}

	fcntl(fd[0], F_SETFL, O_NONBLOCK);
	fcntl(fd[1], F_SETFL, O_NONBLOCK);
	handle_trm(in, out, out, fd[1], ctl, info, len);
	mem_free(info);

	term = init_term(fd[0], out, win_func);
	if (term) {
		/* OK, this is race condition, but it must be so; GPM installs
		 * it's own buggy TSTP handler. */
		handle_basic_signals(term);

		return fd[1];
	}

	close(fd[0]);
	close(fd[1]);

	return -1;
}


int ac;
unsigned char **av;
unsigned char *path_to_exe;
int init_b = 0;


void
init()
{
	int uh;
	void *info;
	int len;
	unsigned char *u;
	
	init_trans();
	set_sigcld();
	init_home();
	init_keymaps();

	/* Parsing command line options */
	u = parse_options(ac - 1, av + 1);
	if (!u) {
		retval = RET_SYNTAX;
		terminate = 1;
		return;
	}
	
	/* If there's no -no-connect option, check if there's no other ELinks
	 * running. If we found any, open socket and act as a slave for it. */
	if (!no_connect) { 
		uh = bind_to_af_unix();
		if (uh != -1) {
			info = create_session_info(base_session, u, &len);
			if (!info) {
				retval = RET_FATAL;
				terminate = 1;
				return;
			}

			handle_trm(get_input_handle(), get_output_handle(),
				   uh, uh, get_ctl_handle(), info, len);

			/* OK, this is race condition, but it must be so; GPM
			 * installs it's own buggy TSTP handler. */
			handle_basic_signals(NULL);
			mem_free(info);

			return;
		}
	}

	/* Default codepage settings may be overridden by config file or
	 * command line, so we need to setup them now. XXX: This is a hack,
	 * as we can't do this while initializing the options array. */
	dds.assume_cp = get_cp_index("ISO-8859-1");
	if (dds.assume_cp == -1) dds.assume_cp = 0;

	load_config();
	init_b = 1;
	read_bookmarks();
	load_url_history();
	init_cookies();
#ifdef HAVE_LUA
    	init_lua();
#endif
	/* Parse commandline options again, in order to override any config
	 * file options. */
	u = parse_options(ac - 1, av + 1);
	if (!u) {
		retval = RET_SYNTAX;
		terminate = 1;
		return;
	}
	
	if (dmp) {
		dump_start(u);
		return;

	} else {
		int attached;

		info = create_session_info(base_session, u, &len);
		if (!info) goto fatal_error;
		
		attached = attach_terminal(get_input_handle(),
					   get_output_handle(),
					   get_ctl_handle(), info, len);
		
		if (attached == -1) {
fatal_error:
			retval = RET_FATAL;
			terminate = 1;
			return;
		}
	}
}


void
terminate_all_subsystems()
{
	af_unix_close();
	destroy_all_sessions();
	check_bottom_halves();
	abort_all_downloads();
	check_bottom_halves();
	destroy_all_terminals();
	check_bottom_halves();
	free_all_itrms();
	abort_all_connections();
#ifdef HAVE_SSL
	ssl_finish();
#endif
#ifdef HAVE_LUA
	if (init_b && !prepare_lua(NULL)) {
		lua_dostring(lua_state, "if quit_hook then quit_hook() end");
		finish_lua();
	}
#endif
	shrink_memory(1);
	if (init_b) save_url_history();
	free_history_lists();
	free_term_specs();
	free_types();
	free_auth();
	if (init_b) finalize_bookmarks();
	free_keymaps();
	free_conv_table();
	free_blacklist();
	if (init_b) cleanup_cookies();
	check_bottom_halves();
	end_config();
	free_strerror_buf();
	shutdown_trans();
	terminate_osdep();
}


int
main(int argc, char *argv[])
{
	path_to_exe = argv[0];
	ac = argc;
	av = (unsigned char **)argv;

	select_loop(init);
	terminate_all_subsystems();

#ifdef LEAK_DEBUG
	check_memory_leaks();
#endif
	return retval;
}


void
shrink_memory(int u)
{
	shrink_dns_cache(u);
	shrink_format_cache(u);
	garbage_collection(u);
	delete_unused_format_cache_entries();
}
