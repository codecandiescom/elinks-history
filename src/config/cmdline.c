/* Command line processing */
/* $Id: cmdline.c,v 1.104 2004/11/08 22:11:47 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

/* We need to have it here. Stupid BSD. */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "config/cmdline.h"
#include "config/conf.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/dns.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "util/error.h"
#include "util/file.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"

/* Hack to handle URL extraction for -remote commands */
static unsigned char *remote_url;

static int
parse_options_(int argc, unsigned char *argv[], struct option *opt,
               struct list_head *url_list)
{
	while (argc) {
		argv++, argc--;

		if (argv[-1][0] == '-' && argv[-1][1]) {
			struct option *option;
			unsigned char *argname = &argv[-1][1];
			unsigned char *oname = stracpy(argname);
			unsigned char *err;

			if (!oname) continue;

			/* Treat --foo same as -foo. */
			if (argname[0] == '-') argname++;

			option = get_opt_rec(opt, argname);
			if (!option) option = get_opt_rec(opt, oname);
			if (!option) {
				unsigned char *pos;

				oname++; /* the '-' */
				/* Substitute '-' by '_'. This helps
				 * compatibility with that very wicked browser
				 * called 'lynx'. */
				for (pos = strchr(oname, '_'); pos;
				     pos = strchr(pos, '_'))
					*pos = '-';
				option = get_opt_rec(opt, oname);
				oname--;
			}

			mem_free(oname);

			if (!option) goto unknown_option;

			if (!option_types[option->type].cmdline)
				goto unknown_option;

			err = option_types[option->type].cmdline(option, &argv, &argc);

			if (err) {
				if (err[0])
					usrerror(gettext("Cannot parse option %s: %s"), argv[-1], err);

				return 1;

			} else if (remote_url) {
				if (url_list) add_to_string_list(url_list, remote_url, -1);
				mem_free(remote_url);
				remote_url = NULL;
			}

		} else if (url_list) {
			add_to_string_list(url_list, argv[-1], -1);
		}
	}

	return 0;

unknown_option:
	usrerror(gettext("Unknown option %s"), argv[-1]);
	return 1;
}

int
parse_options(int argc, unsigned char *argv[], struct list_head *url_list)
{
	return parse_options_(argc, argv, cmdline_options, url_list);
}


/**********************************************************************
 Options handlers
**********************************************************************/

static unsigned char *
eval_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	if (*argc < 1) return gettext("Parameter expected");

	(*argv)++; (*argc)--;	/* Consume next argument */

	parse_config_file(config_options, "-eval", *(*argv - 1), NULL);

	fflush(stdout);

	return NULL;
}

static unsigned char *
forcehtml_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	safe_strncpy(get_opt_str("mime.default_type"), "text/html", MAX_STR_LEN);
	return NULL;
}

static unsigned char *
lookup_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	struct sockaddr_storage *addrs = NULL;
	int addrno, i;

	if (!*argc) return gettext("Parameter expected");
	if (*argc > 1) return gettext("Too many parameters");

	(*argv)++; (*argc)--;
	if (do_real_lookup(*(*argv - 1), &addrs, &addrno, 0)) {
#ifdef HAVE_HERROR
		herror(gettext("error"));
#else
		usrerror(gettext("Host not found"));
#endif
		return "";
	}

	for (i = 0; i < addrno; i++) {
#ifdef CONFIG_IPV6
		struct sockaddr_in6 addr = *((struct sockaddr_in6 *) &(addrs)[i]);
		unsigned char p[INET6_ADDRSTRLEN];

		if (! inet_ntop(addr.sin6_family,
				(addr.sin6_family == AF_INET6 ? (void *) &addr.sin6_addr
							      : (void *) &((struct sockaddr_in *) &addr)->sin_addr),
				p, INET6_ADDRSTRLEN))
			ERROR(gettext("Resolver error"));
		else
			printf("%s\n", p);
#else
		struct sockaddr_in addr = *((struct sockaddr_in *) &(addrs)[i]);
		unsigned char *p = (unsigned char *) &addr.sin_addr.s_addr;

		printf("%d.%d.%d.%d\n", (int) p[0], (int) p[1],
				        (int) p[2], (int) p[3]);
#endif
	}

	mem_free_if(addrs);

	fflush(stdout);

	return "";
}

#define skipback_whitespace(start, S) \
	while ((start) < (S) && isspace((S)[-1])) (S)--;

static unsigned char *
remote_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	struct {
		unsigned char *name;
		enum {
			REMOTE_METHOD_OPENURL,
			REMOTE_METHOD_PING,
			REMOTE_METHOD_XFEDOCOMMAND,
			REMOTE_METHOD_ADDBOOKMARK,
			REMOTE_METHOD_NOT_SUPPORTED,
		} type;
	} remote_methods[] = {
		{ "openURL",	  REMOTE_METHOD_OPENURL },
		{ "ping",	  REMOTE_METHOD_PING },
		{ "addBookmark",  REMOTE_METHOD_ADDBOOKMARK },
		{ "xfeDoCommand", REMOTE_METHOD_XFEDOCOMMAND },
		{ NULL,		  REMOTE_METHOD_NOT_SUPPORTED },
	};
	unsigned char *command, *arg, *argend = NULL;
	int method, len = 0;

	if (*argc < 1) return gettext("Parameter expected");

	command = *(*argv);

	while (isalpha(command[len]))
		len++;

	arg = strchr(&command[len], '(');
	if (arg) {
		arg++;
		skip_space(arg);

		argend = strchr(arg, ')');
	}

	if (!argend) {
		/* Just open any passed URLs in new tabs */
		remote_session_flags |= SES_REMOTE_NEW_TAB;
		return NULL;
	}

	skipback_whitespace(arg, argend);

	for (method = 0; remote_methods[method].name; method++) {
		unsigned char *name = remote_methods[method].name;

		if (!strlcasecmp(command, len, name, -1))
			break;
	}

	switch (remote_methods[method].type) {
	case REMOTE_METHOD_OPENURL:
		if (arg == argend) {
			/* Prompt for a URL with a dialog box */
			remote_session_flags |= SES_REMOTE_PROMPT_URL;
			break;
		}

		len = strcspn(arg, ",)");
		if (arg[len] == ',') {
			unsigned char *where = arg + len + 1;

			if (strstr(where, "new-window")) {
				remote_session_flags |= SES_REMOTE_NEW_WINDOW;

			} else if (strstr(where, "new-tab")) {
				remote_session_flags |= SES_REMOTE_NEW_TAB;

			} else {
				/* Bail out when getting unknown parameter */
				/* TODO: new-screen */
				break;
			}

		} else {
			remote_session_flags |= SES_REMOTE_CURRENT_TAB;
		}

		while (len > 0 && isspace(arg[len - 1])) len--;
		if (len) remote_url = memacpy(arg, len);

		break;

	case REMOTE_METHOD_XFEDOCOMMAND:
		len = argend - arg;

		if (!strlcasecmp(arg, len, "openBrowser", 11)) {
			remote_session_flags = SES_REMOTE_NEW_WINDOW;
		}
		break;

	case REMOTE_METHOD_PING:
		remote_session_flags = SES_REMOTE_PING;
		break;

	case REMOTE_METHOD_ADDBOOKMARK:
		if (arg == argend) break;
		remote_url = memacpy(arg, argend - arg);
		remote_session_flags = SES_REMOTE_ADD_BOOKMARK;
		break;

	case REMOTE_METHOD_NOT_SUPPORTED:
		break;
	}

	/* If no flags was applied it can only mean we are dealing with
	 * unknown method. */
	if (!remote_session_flags)
		return gettext("Remote method not supported");

	(*argv)++; (*argc)--;	/* Consume next argument */

	return NULL;
}

static unsigned char *
version_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	printf("%s\n", full_static_version);
	fflush(stdout);
	return "";
}


/* Below we handle help usage printing.
 *
 * We're trying to achieve several goals here:
 *
 * - Genericly define a function to print option trees iteratively.
 * - Make output parsable for various doc tools (to make manpages).
 * - Do some non generic fancy stuff like printing semi-aliased
 *   options (like: -?, -h and -help) on one line when printing
 *   short help. */

#define gettext_nonempty(x) (*(x) ? gettext(x) : (x))

static void
print_full_help(struct option *tree, unsigned char *path)
{
	struct option *option;
	unsigned char saved[MAX_STR_LEN];
	unsigned char *savedpos = saved;

	*savedpos = 0;

	foreach (option, *tree->value.tree) {
		enum option_type type = option->type;
		unsigned char *help;
		unsigned char *capt = option->capt;
		unsigned char *desc = (option->desc && *option->desc)
				      ? (unsigned char *) gettext(option->desc)
				      : (unsigned char *) "N/A";

		/* Don't print deprecated aliases (if we don't walk command
		 * line options which use aliases for legitimate options). */
		if ((type == OPT_ALIAS && tree != cmdline_options)
		    || (option->flags & OPT_HIDDEN))
			continue;

		if (!capt && !strncasecmp(option->name, "_template_", 10))
			capt = (unsigned char *) N_("Template option folder");

		if (!capt) {
			int len = strlen(option->name);
			int max = MAX_STR_LEN - (savedpos - saved);

			safe_strncpy(savedpos, option->name, max);
			safe_strncpy(savedpos + len, ", -", max - len);
			savedpos += len + 3;
			continue;
		}

		help = gettext_nonempty(option_types[option->type].help_str);

		if (type != OPT_TREE)
			printf("    %s%s%s %s ",
				path, saved, option->name, help);

		/* Print the 'title' of each option type. */
		switch (type) {
			case OPT_BOOL:
			case OPT_INT:
			case OPT_LONG:
				printf(gettext("(default: %ld)"),
					type == OPT_LONG
					? option->value.big_number
					: (long) option->value.number);
				break;

			case OPT_STRING:
				printf(gettext("(default: \"%s\")"),
					option->value.string);
				break;

			case OPT_ALIAS:
				printf(gettext("(alias for %s)"),
					option->value.string);
				break;

			case OPT_CODEPAGE:
				printf(gettext("(default: %s)"),
					get_cp_name(option->value.number));
				break;

			case OPT_COLOR:
			{
				color_t color = option->value.color;
				unsigned char hexcolor[8];

				printf(gettext("(default: %s)"),
				       get_color_string(color, hexcolor));
				break;
			}

			case OPT_COMMAND:
				break;

			case OPT_LANGUAGE:
#ifdef ENABLE_NLS
				printf(gettext("(default: \"%s\")"),
				       language_to_name(option->value.number));
#endif
				break;

			case OPT_TREE:
			{
				int pathlen = strlen(path);
				int namelen = strlen(option->name);

				if (pathlen + namelen + 2 > MAX_STR_LEN)
					continue;

				/* Append option name to path */
				if (pathlen > 0) {
					memcpy(saved, path, pathlen);
					savedpos = saved + pathlen;
				} else {
					savedpos = saved;
				}
				memcpy(savedpos, option->name, namelen + 1);
				savedpos += namelen;

				capt = gettext_nonempty(capt);
				printf("  %s: (%s)", capt, saved);
				break;
			}
		}

		printf("\n    %8s", "");
		{
			int l = strlen(desc);
			int i;

			for (i = 0; i < l; i++) {
				putchar(desc[i]);

				if (desc[i] == '\n')
					printf("    %8s", "");
			}
		}
		printf("\n\n");

		if (option->type == OPT_TREE) {
			memcpy(savedpos, ".", 2);
			print_full_help(option, saved);
		}

		savedpos = saved;
		*savedpos = 0;
	}
}

static void
print_short_help()
{
#define ALIGN_WIDTH 20
	struct option *option;
	struct string string = NULL_STRING;
	struct string *saved = NULL;
	unsigned char align[ALIGN_WIDTH];

	/* Initialize @space used to align captions. */
	memset(align, ' ', sizeof(align) - 1);
	align[sizeof(align) - 1] = 0;

	foreach (option, *cmdline_options->value.tree) {
		unsigned char *capt;
		unsigned char *help;
		unsigned char *info = saved ? saved->source
					    : (unsigned char *) "";
		int len = strlen(option->name);

		/* Avoid printing compatibility options */
		if (option->flags & OPT_HIDDEN)
			continue;

		/* When no caption is available the option name is 'stacked'
		 * and the caption is shared with next options that has one. */
		if (!option->capt) {
			if (!saved) {
				if (!init_string(&string))
					continue;

				saved = &string;
			}

			add_to_string(saved, option->name);
			add_to_string(saved, ", -");
			continue;
		}

		capt = gettext_nonempty(option->capt);
		help = gettext_nonempty(option_types[option->type].help_str);

		/* When @help string is non empty align at least one space. */
		len = ALIGN_WIDTH - len - strlen(help);
		len -= (saved ? saved->length : 0);
		len = (len < 0) ? !!(*help) : len;

		align[len] = '\0';
		printf("  -%s%s %s%s%s\n",
		       info, option->name, help, align, capt);
		align[len] = ' ';
		if (saved) {
			done_string(saved);
			saved = NULL;
		}
	}
#undef ALIGN_WIDTH
}

#undef gettext_nonempty

static unsigned char *
printhelp_cmd(struct option *option, unsigned char ***argv, int *argc)
{
	version_cmd(NULL, NULL, NULL);
	printf("\n");

	if (!strcmp(option->name, "config-help")) {
		printf("%s:\n", gettext("Configuration options"));
		print_full_help(config_options, "");
	} else {
		printf("%s\n\n%s:\n",
		       gettext("Usage: elinks [OPTION]... [URL]"),
		       gettext("Options"));
		if (!strcmp(option->name, "long-help")) {
			print_full_help(cmdline_options, "-");
		} else {
			print_short_help();
		}
	}

	fflush(stdout);
	return "";
}

static unsigned char *
redir_cmd(struct option *option, unsigned char ***argv, int *argc)
{
	unsigned char *target;

	/* I can't get any dirtier. --pasky */

	if (!strcmp(option->name, "confdir")) {
		target = "config-dir";
	} else if (!strcmp(option->name, "conffile")) {
		target = "config-file";
	} else if (!strcmp(option->name, "stdin")) {
		static int complained;

		if (complained)
			return NULL;
		complained = 1;

		/* Emulate bool option, possibly eating following 0/1. */
		if ((*argv)[0]
		    && ((*argv)[0][0] == '0' || (*argv)[0][0] == '1')
		    && !(*argv)[0][1])
			(*argv)++, (*argc)--;
		fprintf(stderr, "Warning: Deprecated option -stdin used!\n");
		fprintf(stderr, "ELinks now determines the -stdin option value automatically.\n");
		fprintf(stderr, "In the future versions ELinks will report error when you will\n");
		fprintf(stderr, "continue to use this option.\a\n");
		return NULL;

	} else {
		return gettext("Internal consistency error");
	}

	option = get_opt_rec(cmdline_options, target);
	assert(option);
	option_types[option->type].cmdline(option, argv, argc);
	return NULL;
}


/**********************************************************************
 Options values
**********************************************************************/

/* Keep options in alphabetical order. */

struct option_info cmdline_options_info[] = {
	INIT_OPT_BOOL("", N_("Restrict to anonymous mode"),
		"anonymous", 0, 0,
		N_("Restrict ELinks so that it can run on an anonymous account.\n"
		"No local file browsing, no downloads. Execution of viewers\n"
		"is allowed, but user can't add or modify entries in\n"
		"association table.")),

	INIT_OPT_BOOL("", N_("Autosubmit first form"),
		"auto-submit", 0, 0,
		N_("Go and submit the first form you'll stumble upon.")),

	INIT_OPT_INT("", N_("Clone session with given ID"),
		"base-session", 0, 0, INT_MAX, 0,
		N_("ID of session (ELinks instance) which we want to clone.\n"
		"This is internal ELinks option, you don't want to use it.")),

	INIT_OPT_COMMAND("", NULL, "confdir", OPT_HIDDEN, redir_cmd, NULL),

	INIT_OPT_STRING("", N_("Name of directory with configuration file"),
		"config-dir", 0, "",
		N_("ELinks will read and write its config files to the given\n"
		"directory instead of ~/.elinks. If the path begins with\n"
		"a '/' its used as an absolute path. Else it is assumed to\n"
		"be relative to your HOME dir.")),

	INIT_OPT_COMMAND("", NULL, "conffile", OPT_HIDDEN, redir_cmd, NULL),

	INIT_OPT_STRING("", N_("Name of configuration file"),
		"config-file", 0, "elinks.conf",
		N_("Name of the file with configuration, from which and to\n"
		"which all the configuration shall be written. It should be\n"
		"relative to config-dir.")),

	INIT_OPT_COMMAND("", N_("Print help for configuration options"),
		"config-help", 0, printhelp_cmd,
		N_("Print help on configuration options and exit.")),

	INIT_OPT_CMDALIAS("", N_("MIME type to assume for documents"),
		"default-mime-type", 0, "mime.default_type",
		N_("Default MIME type to assume for documents of unknown type.")),

	INIT_OPT_BOOL("", N_("Ignore user-defined keybindings"),
		"default-keys", 0, 0,
		N_("If set, parse but don't set keybindings from configuration\n"
		   "file. It forces use of default keybindings and will reset\n"
		   "user-defined ones on save.")),

	INIT_OPT_BOOL("", N_("Write formatted version of given URL to stdout"),
		"dump", 0, 0,
		N_("Write a plain-text version of the given HTML document to\n"
		"stdout.")),

	INIT_OPT_CMDALIAS("", N_("Codepage to use with -dump"),
		"dump-charset", 0, "document.dump.codepage",
		N_("Codepage used in dump output.")),

	INIT_OPT_CMDALIAS("", N_("Width of document formatted with -dump"),
		"dump-width", 0, "document.dump.width",
		N_("Width of the dump output.")),

	INIT_OPT_COMMAND("", N_("Evaluate given configuration option"),
		"eval", 0, eval_cmd,
		N_("Specify elinks.conf config options on the command-line:\n"
		"  -eval 'set protocol.file.allow_special_files = 1'")),

	/* lynx compatibility */
	INIT_OPT_COMMAND("", N_("Assume the file is HTML"),
		"force-html", 0, forcehtml_cmd,
		N_("This makes ELinks assume that the files it sees are HTML. This is\n"
		"equivalent to -default-mime-type text/html.")),

	/* XXX: -?, -h and -help share the same caption and should be kept in
	 * the current order for usage help printing to be ok */
	INIT_OPT_COMMAND("", NULL, "?", 0, printhelp_cmd, NULL),

	INIT_OPT_COMMAND("", NULL, "h", 0, printhelp_cmd, NULL),

	INIT_OPT_COMMAND("", N_("Print usage help and exit"),
		"help", 0, printhelp_cmd,
		N_("Print usage help and exit.")),

	INIT_OPT_COMMAND("", N_("Print detailed usage help and exit"),
		"long-help", 0, printhelp_cmd,
		N_("Print detailed usage help and exit.")),

	INIT_OPT_BOOL("", N_("Only permit local connections"),
		"localhost", 0, 0,
		N_("Restrict ELinks so that it can only open connections to\n"
		"local addresses (ie. 127.0.0.1), it will prevent any connection\n"
		"to distant servers.")),

	INIT_OPT_COMMAND("", N_("Look up specified host"),
		"lookup", 0, lookup_cmd,
		N_("Look up specified host.")),

	INIT_OPT_BOOL("", N_("Run as separate instance"),
		"no-connect", 0, 0,
		N_("Run ELinks as a separate instance instead of connecting to an\n"
		"existing instance. Note that normally no runtime state files\n"
		"(bookmarks, history and so on) are written to the disk when\n"
		"this option is used. See also -touch-files.")),

	INIT_OPT_BOOL("", N_("Don't use files in ~/.elinks"),
		"no-home", 0, 0,
		N_("Don't attempt to create and/or use home rc directory (~/.elinks).")),

	INIT_OPT_BOOL("", N_("Do not number links in dump output"),
		"no-numbering", 0, 0,
		N_("Prevents numbering of links in the -dump output; this was the\n"
		   "default behaviour until 0.5pre12.\n"
		   "Note that this really affects only -dump, nothing else.")),

	INIT_OPT_BOOL("", N_("Do not dump links references in dump output"),
		"no-references", 0, 0,
		N_("Prevents dumping of links references (URIs) in the -dump output.\n"
		   "Note that this really affects only -dump, nothing else.")),

	INIT_OPT_COMMAND("", N_("Control an already running ELinks"),
		"remote", 0, remote_cmd,
		N_("Mozilla compliant support for controlling a remote ELinks instance.\n"
		"The command takes an additional argument containing the method which\n"
		"should be invoked and any parameters that should be passed to it.\n"
		"For ease of use the additional method argument can be omitted in which\n"
		"case any URL arguments will be opened in new tabs in the remote instance.\n"
		"Following is a list of the methods that are supported:\n"
		"  ping()                    -- check for remote instance\n"
		"  openURL()                 -- prompt URL in current tab\n"
		"  openURL(URL)              -- open URL in current tab\n"
		"  openURL(URL, new-tab)     -- open URL in new tab\n"
		"  openURL(URL, new-window)  -- open URL in new window\n"
		"  addBookmark(URL)          -- bookmark URL\n"
		"  xfeDoCommand(openBrowser) -- open new window")),

	INIT_OPT_INT("", N_("Connect to session ring with given ID"),
		"session-ring", 0, 0, INT_MAX, 0,
		N_("ID of session ring this ELinks session should connect to. ELinks\n"
		"works in so-called session rings, whereby all instances of ELinks\n"
		"are interconnected and share state (cache, bookmarks, cookies,\n"
		"and so on). By default, all ELinks instances connect to session\n"
		"ring 0. You can change that behaviour with this switch and form as\n"
		"many session rings as you want. Obviously, if the session-ring with\n"
		"this number doesn't exist yet, it's created and this ELinks instance\n"
		"will become the master instance (that usually doesn't matter for you\n"
		"as a user much). Note that you usually don't want to use this unless\n"
		"you're a developer and you want to do some testing - if you want the\n"
		"ELinks instances each running standalone, rather use the -no-connect\n"
		"command-line option. Also note that normally no runtime state files\n"
		"are written to the disk when this option is used. See also\n"
		"-touch-files.")),

	INIT_OPT_BOOL("", N_("Write the source of given URL to stdout"),
		"source", 0, 0,
		N_("Write the given HTML document in source form to stdout.")),

	INIT_OPT_COMMAND("", NULL, "stdin", OPT_HIDDEN, redir_cmd, NULL),

	INIT_OPT_BOOL("", N_("Touch files in ~/.elinks when running with -no-connect/-session-ring"),
		"touch-files", 0, 0,
		N_("Set to 1 to have runtime state files (bookmarks, history, ...)\n"
		"changed even when -no-connect or -session-ring is used; has no\n"
		"effect if not used in connection with any of these options.")),

	INIT_OPT_INT("", N_("Verbose level"),
		"verbose", 0, 0, VERBOSE_LEVELS - 1, 0,
		N_("The verbose level controls what messages are shown at\n"
		"start up and while running:\n"
		"	0 means only show serious errors\n"
		"	1 means show serious errors and warnings\n"
		"	2 means show all messages\n")),

	INIT_OPT_COMMAND("", N_("Print version information and exit"),
		"version", 0, version_cmd,
		N_("Print ELinks version information and exit.")),

	NULL_OPTION_INFO,
};
