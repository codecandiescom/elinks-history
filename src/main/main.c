/* The main program - startup */
/* $Id: main.c,v 1.92 2003/05/24 20:16:54 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "main.h"
#ifdef USE_LEDS
#include "bfu/leds.h"
#endif
#include "bookmarks/bookmarks.h"
#include "config/cmdline.h"
#include "config/conf.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "config/urlhist.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/auth.h"
#include "document/cache.h"
#include "document/html/colors.h"
#include "document/html/renderer.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/af_unix.h"
#include "lowlevel/dns.h"
#include "lowlevel/home.h"
#include "terminal/kbd.h"
#include "lowlevel/select.h"
#include "lowlevel/signals.h"
#include "lowlevel/sysname.h"
#include "terminal/terminal.h"
#include "lowlevel/timer.h"
#include "lua/core.h"
#include "lua/hooks.h"
#include "protocol/mailcap.h"
#include "protocol/mime.h"
#include "protocol/http/auth.h"
#include "sched/download.h"
#include "sched/sched.h"
#include "sched/session.h"
#include "ssl/ssl.h"
#include "util/blacklist.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "version.h"
#include "viewer/dump/dump.h"

int terminate = 0;
enum retval retval = RET_OK;

/* TODO: I'd like to have this rather somewhere in lowlevel/. --pasky */

static int terminal_pipe[2];

static int
attach_terminal(int in, int out, int ctl, void *info, int len)
{
	struct terminal *term;

	if (set_nonblocking_fd(terminal_pipe[0]) < 0) return -1;
	if (set_nonblocking_fd(terminal_pipe[1]) < 0) return -1;
	handle_trm(in, out, out, terminal_pipe[1], ctl, info, len);

	mem_free(info);

	term = init_term(terminal_pipe[0], out, tabwin_func);
	if (!term) {
		close(terminal_pipe[0]);
		close(terminal_pipe[1]);

		return -1;
	}

	/* OK, this is race condition, but it must be so; GPM installs it's own
	 * buggy TSTP handler. */
	handle_basic_signals(term);

	return terminal_pipe[1];
}


unsigned char *path_to_exe;

static int ac;
static unsigned char **av;
static int init_b = 0;


void
init(void)
{
	int uh;
	void *info;
	int len;
	unsigned char *u = NULL;

	init_static_version();

#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");
#endif
#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	set_language(0);
#endif

	init_options();
	set_sigcld();
	get_system_name();
	init_keymaps();

	/* XXX: OS/2 has some stupid bug and the pipe must be created before
	 * socket :-/. -- Mikulas */
	if (c_pipe(terminal_pipe)) {
		error("ERROR: can't create pipe for internal communication");
		goto fatal_error;
	}

	/* Parsing command line options */
	u = parse_options(ac - 1, av + 1);
	if (!u) {
		retval = RET_SYNTAX;
		terminate = 1;
		return;
	}
	u = stracpy(u);
	if (!u) goto fatal_error;

	if (!get_opt_bool_tree(&cmdline_options, "no-home")) {
		init_home();
	}

	/* If there's no -no-connect option, check if there's no other ELinks
	 * running. If we found any, open socket and act as a slave for it. */
	while (!get_opt_bool_tree(&cmdline_options, "no-connect")
		&& !get_opt_bool_tree(&cmdline_options, "dump")
		&& !get_opt_bool_tree(&cmdline_options, "source")) {

		uh = bind_to_af_unix();
		if (uh < 0) break;

		close(terminal_pipe[0]);
		close(terminal_pipe[1]);

		info = create_session_info(get_opt_int_tree(&cmdline_options,
							    "base-session"),
					   u, &len);
		mem_free(u), u = NULL;
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

	load_config();
	/* Parse commandline options again, in order to override any config
	 * file options. */
	parse_options(ac - 1, av + 1);

	init_b = 1;
#ifdef USE_LEDS
	init_leds();
#endif
	init_timer();
#ifdef BOOKMARKS
	read_bookmarks();
#endif
#ifdef GLOBHIST
	read_global_history();
#endif
	load_url_history();
#ifdef COOKIES
	init_cookies();
#endif
#ifdef MAILCAP
	mailcap_init();
#endif
	init_ssl();
#ifdef HAVE_LUA
    	init_lua();
#endif

	if (get_opt_int_tree(&cmdline_options, "dump") ||
	    get_opt_int_tree(&cmdline_options, "source")) {
		if (get_opt_bool_tree(&cmdline_options, "stdin")) {
			get_opt_bool("protocol.file.allow_special_files") = 1;
			mem_free(u);
			u = stracpy("file:///dev/stdin");
			if (!u) goto fatal_error;
		}

		dump_start(u);
		mem_free(u), u = NULL;
		if (terminate) {
			/* XXX? */
			close(terminal_pipe[0]);
			close(terminal_pipe[1]);
		}
		return;

	} else {
		int attached;

		info = create_session_info(get_opt_int_tree(&cmdline_options, "base-session"), u, &len);
		mem_free(u), u = NULL;
		if (!info) goto fatal_error;

		attached = attach_terminal(get_input_handle(),
					   get_output_handle(),
					   get_ctl_handle(), info, len);

		if (attached == -1) {
fatal_error:
			if (u) mem_free(u), u = NULL; /* Just in case... */
			retval = RET_FATAL;
			terminate = 1;
			return;
		}
	}
}


static void
terminate_all_subsystems(void)
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
	done_ssl();

	if (init_b) {
#ifdef HAVE_SCRIPTING
		script_hook_quit();
#endif
		save_url_history();
#ifdef GLOBHIST
		finalize_global_history();
#endif
#ifdef BOOKMARKS
		finalize_bookmarks();
#endif
#ifdef COOKIES
		cleanup_cookies();
#endif
#ifdef MAILCAP
		mailcap_exit();
#endif
#ifdef HAVE_LUA
		cleanup_lua();
#endif
	}

	shrink_memory(1);

	free_table_cache();
	free_history_lists();
	free_auth();
	free_keymaps();
	free_conv_table();
	free_blacklist();
	check_bottom_halves();
	free_home();
	free_strerror_buf();
#ifdef USE_LEDS
	done_leds();
#endif
	done_timer();
	done_options();
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
