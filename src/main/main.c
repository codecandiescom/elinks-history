/* The main program - startup */
/* $Id: main.c,v 1.186 2004/04/14 05:35:03 jonas Exp $ */

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

int ac;
unsigned char **av;
static int init_b = 0;

void
init(void)
{
	int ret, fd = -1;

	INIT_LIST_HEAD(url_list);

	init_static_version();

#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");
#endif
#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	set_language(0);
#endif

	init_charsets_lookup();
	init_colors_lookup();
	init_modules(main_modules);

	init_event();
	init_options();
	register_modules_options(main_modules);
	register_modules_options(builtin_modules);
	set_sigcld();
	get_system_name();
	init_keymaps();

	/* XXX: OS/2 has some stupid bug and the pipe must be created before
	 * socket :-/. -- Mikulas */
	if (check_terminal_pipes()) {
		ERROR(gettext("Cannot create a pipe for internal communication."));
		goto fatal_error;
	}

	/* Parsing command line options */
	ret = parse_options(ac - 1, av + 1, &url_list);
	if (ret) {
		retval = RET_SYNTAX;
		terminate = 1;
		goto end;
	}

	/* FIXME: This is almost perfect. Only problem that remains is piping
	 * stuff from stdin to be rendered in the text viewer this is possible
	 * but requires one to pass -eval allowing special files because we use
	 * /dev/stdin. It would be cool if either always allowed to read from
	 * /dev/stdin in read_encoded_file(). Actually we already do that for
	 * -dump and -source but setting protocol.file.allow_special_files
	 * could be dangerous when not using the dump viewer. --jonas */
	if (!isatty(STDIN_FILENO)) {
		add_to_string_list(&url_list, "file:///dev/stdin", 17);
		get_opt_bool_tree(cmdline_options, "no-connect") = 1;
	}

	/* If called for outputting to a pipe without -dump or -source
	 * specified default to using dump viewer. */
	if (!isatty(STDOUT_FILENO)) {
		int *dump = &get_opt_bool_tree(cmdline_options, "dump");

		if (!*dump && !get_opt_bool_tree(cmdline_options, "source"))
			*dump = 1;
	}

	if (!get_opt_bool_tree(cmdline_options, "no-home")) {
		init_home();
	}

	/* If there's no -no-connect option, check if there's no other ELinks
	 * running. If we found any, open socket and act as a slave for it. */

	if (get_opt_bool_tree(cmdline_options, "no-connect")
	    || get_opt_bool_tree(cmdline_options, "dump")
	    || get_opt_bool_tree(cmdline_options, "source")
	    || (fd = af_unix_open()) == -1) {

		load_config();
		/* Parse commandline options again, in order to override any config
		 * file options. */
		parse_options(ac - 1, av + 1, NULL);

		init_b = 1;
		init_modules(builtin_modules);
		init_timer();
		load_url_history();
		init_search_history();
	}

	if (get_opt_int_tree(cmdline_options, "dump") ||
	    get_opt_int_tree(cmdline_options, "source")) {
		dump_pre_start(&url_list);
		if (terminate) {
			/* XXX? */
			close_terminal_pipes();
		}
	} else {
		int len;
		int id = get_opt_int_tree(cmdline_options, "base-session");
		void *info = create_session_info(id, &url_list, &len);

		if (!info) goto fatal_error;

		if (fd != -1) {
			close_terminal_pipes();

			handle_trm(get_input_handle(), get_output_handle(),
				   fd, fd, get_ctl_handle(), info, len);

			/* OK, this is race condition, but it must be so; GPM
			 * installs it's own buggy TSTP handler. */
			handle_basic_signals(NULL);
			mem_free(info);
			goto end;
		}

		fd = attach_terminal(get_input_handle(), get_output_handle(),
				     get_ctl_handle(), info, len);
		if (fd == -1) {
fatal_error:
			retval = RET_FATAL;
			terminate = 1;
		}
	}
end:
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
#ifdef HAVE_SCRIPTING
		trigger_event_name("quit");
#endif
		save_url_history();
		done_search_history();
		free_marks();
		done_modules(builtin_modules);
	}

	shrink_memory(1);
	free_charsets_lookup();
	free_colors_lookup();
	done_modules(main_modules);
	free_history_lists();
	free_auth();
	free_keymaps();
	free_conv_table();
	free_blacklist();
	check_bottom_halves();
	free_home();
	free_strerror_buf();
	done_screen_drivers();
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
