/* The main program - startup */
/* $Id: main.c,v 1.214 2004/06/17 10:02:20 zas Exp $ */

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

#include "bfu/style.h"
#include "config/cmdline.h"
#include "config/conf.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "config/urlhist.h"
#include "dialogs/menu.h"
#include "cache/cache.h"
#include "document/document.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/dns.h"
#include "lowlevel/home.h"
#include "lowlevel/interlink.h"
#include "lowlevel/select.h"
#include "lowlevel/signals.h"
#include "lowlevel/sysname.h"
#include "lowlevel/timer.h"
#include "main.h"
#include "modules/module.h"
#include "modules/version.h"
#include "osdep/osdep.h"
#include "protocol/auth/auth.h"
#include "protocol/http/blacklist.h"
#include "sched/connection.h"
#include "sched/download.h"
#include "sched/error.h"
#include "sched/event.h"
#include "sched/session.h"
#include "terminal/kbd.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "viewer/dump/dump.h"
#include "viewer/text/marks.h"
#include "viewer/text/search.h"

int terminate = 0;
enum retval retval = RET_OK;
unsigned char *path_to_exe;

static int ac;
static unsigned char **av;
static int init_b = 0;

static void
init(void)
{
	INIT_LIST_HEAD(url_list);
	int ret, fd = -1;

	init_static_version();

#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");
#endif
#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	set_language(0);
#endif

	init_event();

	init_charsets_lookup();
	init_colors_lookup();
	init_modules(main_modules);

	init_options();
	register_modules_options(main_modules);
	register_modules_options(builtin_modules);
	set_sigcld();
	get_system_name();
	init_keymaps();

	/* XXX: OS/2 has some stupid bug and the pipe must be created before
	 * socket :-/. -- Mikulas */
	if (check_terminal_pipes()) {
		ERROR(G_("Cannot create a pipe for internal communication."));
		retval = RET_FATAL;
		terminate = 1;
		return;
	}

	/* Parsing command line options */
	ret = parse_options(ac - 1, av + 1, &url_list);
	if (ret) {
		retval = RET_SYNTAX;
		terminate = 1;
		free_string_list(&url_list);
		return;
	}

	/* Should the document be read from stdin? */
	if (!isatty(STDIN_FILENO)) {
		add_to_string_list(&url_list, "file:///dev/stdin", 17);
		get_cmd_opt_bool("no-connect") = 1;
	}

	/* If called for outputting to a pipe without -dump or -source
	 * specified default to using dump viewer. */
	if (!isatty(STDOUT_FILENO)) {
		int *dump = &get_cmd_opt_bool("dump");

		if (!*dump && !get_cmd_opt_bool("source"))
			*dump = 1;
	}

	if (!get_cmd_opt_bool("no-home")) {
		init_home();
	}

	/* If there's no -no-connect, -dump or -source option, check if there's
	 * no other ELinks running. If we found any, by-pass initialization of
	 * non critical subsystems, open socket and act as a slave for it. */
	if (get_cmd_opt_bool("no-connect")
	    || get_cmd_opt_bool("dump")
	    || get_cmd_opt_bool("source")
	    || (fd = af_unix_open()) == -1) {

		load_config();
		/* Parse commandline options again, in order to override any
		 * config file options. */
		parse_options(ac - 1, av + 1, NULL);

		init_b = 1;
		init_modules(builtin_modules);
		init_timer();
		load_url_history();
		init_search_history();
	}

	if (get_cmd_opt_int("dump")
	    || get_cmd_opt_int("source")) {
		/* Dump the URL list */
		dump_pre_start(&url_list);

	} else if (remote_session_flags & SES_REMOTE_PING) {
		/* If no instance was running return ping failure */
		if (fd == -1) {
			usrerror(G_("No running ELinks found."));
			retval = RET_PING;
		}
		terminate = 1;

	} else if (remote_session_flags && fd == -1) {
		/* The remote session(s) can not be created */
		usrerror(G_("No remote session to connect to."));
		retval = RET_REMOTE;
		terminate = 1;

	} else {
		struct string info;
		struct terminal *term = NULL;

		if (!encode_session_info(&info, &url_list)) {
			ERROR(G_("Unable to encode session info."));
			retval = RET_FATAL;
			terminate = 1;

		} else if (fd != -1) {
			/* Attach to already running ELinks and act as a slave
			 * for it. */
			close_terminal_pipes();

			handle_trm(get_input_handle(), get_output_handle(),
				   fd, fd, get_ctl_handle(), info.source, info.length,
				   remote_session_flags);
		} else {
			/* Setup a master terminal */
			term = attach_terminal(get_input_handle(), get_output_handle(),
					       get_ctl_handle(), info.source, info.length);
			if (!term) {
				ERROR(G_("Unable to attach_terminal()."));
				retval = RET_FATAL;
				terminate = 1;
			}
		}

		/* OK, this is race condition, but it must be so; GPM installs
		 * it's own buggy TSTP handler. */
		if (!terminate) handle_basic_signals(term);
		done_string(&info);
	}

	if (terminate) close_terminal_pipes();
	free_string_list(&url_list);
}


static void
terminate_all_subsystems(void)
{
	af_unix_close();
	check_bottom_halves();
	abort_all_downloads();
	check_bottom_halves();
	destroy_all_terminals();
	check_bottom_halves();
	free_all_itrms();
	abort_all_connections();

	if (init_b) {
#ifdef CONFIG_SCRIPTING
		trigger_event_name("quit");
#endif
		save_url_history();
		done_search_history();
#ifdef CONFIG_MARKS
		free_marks();
#endif
		free_history_lists();
		free_auth();
		free_blacklist();
		done_modules(builtin_modules);
		done_screen_drivers();
		done_saved_session_info();
	}

	shrink_memory(1);
	free_charsets_lookup();
	free_colors_lookup();
	done_modules(main_modules);
	free_keymaps();
	free_conv_table();
	check_bottom_halves();
	free_home();
	free_strerror_buf();
	done_bfu_colors();
	done_timer();
	unregister_modules_options(builtin_modules);
	unregister_modules_options(main_modules);
	done_options();
	done_event();
	terminate_osdep();
}

void
shrink_memory(int whole)
{
	shrink_dns_cache(whole);
	shrink_format_cache(whole);
	garbage_collection(whole);
}

#ifdef CONFIG_NO_ROOT_EXEC
static void
check_if_root(void)
{
	if (!getuid() || !geteuid()) {
		fprintf(stderr, "%s\n\n"
				"Permission to run this program as root "
				"user was disabled at compile time.\n\n",
				full_static_version);
		exit(-1);
	}
}
#else
#define check_if_root()
#endif

int
main(int argc, char *argv[])
{
	check_if_root();

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
