/* Command line processing */
/* $Id: cmdline.c,v 1.29 2003/12/27 21:56:08 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#include <netdb.h>

/* We need to have it here. Stupid BSD. */
#include <netinet/in.h>
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
#include "util/memory.h"


static unsigned char *
_parse_options(int argc, unsigned char *argv[], struct option *opt)
{
	unsigned char *location = NULL;

	while (argc) {
		argv++, argc--;

		if (argv[-1][0] == '-' && argv[-1][1]) {
			struct option *option;
			unsigned char *argname = &argv[-1][1];
			unsigned char *oname = stracpy(argname);

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

			if (!option) {
				goto unknown_option;
			}

			if (option_types[option->type].cmdline
			    && !(option->flags & OPT_HIDDEN)) {
				unsigned char *err;

				err = option_types[option->type].cmdline(option, &argv, &argc);

				if (err) {
					if (err[0])
						ERROR(gettext("Cannot parse option %s: %s"), argv[-1], err);

					return NULL;
				}
			} else {
				goto unknown_option;
			}

		} else if (!location) {
			location = argv[-1];

		} else {
unknown_option:
			ERROR(gettext("Unknown option %s"), argv[-1]);
			return NULL;
		}
	}

	return empty_string_or_(location);
}

unsigned char *
parse_options(int argc, unsigned char *argv[])
{
	return _parse_options(argc, argv, cmdline_options);
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
		ERROR(gettext("Host not found"));
#endif
		return "";
	}

	for (i = 0; i < addrno; i++) {
#ifdef IPV6
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

	if (addrs) mem_free(addrs);

	fflush(stdout);

	return "";
}

static unsigned char *
version_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	printf(gettext("ELinks %s - Text WWW browser\n"), VERSION_STRING);
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

		/* Don't print deprecated aliases and command line options */
		if (type == OPT_ALIAS && tree != cmdline_options)
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
					(long) option->value.number);
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
				unsigned char *name;

				name = get_color_name(option->value.color);
				if (name) {
					printf(gettext("(default: %s)"), name);
					mem_free(name);
					break;
				}
				printf(gettext("(default: #%06x)"),
					(unsigned int) option->value.color);
			}
				break;

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
		printf("  -%s%s %s%s%s\n", info, option->name, help, align, capt);
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
		printf(gettext("Configuration options:\n"));
		print_full_help(config_options, "");
	} else {
		printf(gettext("Usage: elinks [OPTION]... [URL]\n\n"));
		printf(gettext("Options:\n"));
		if (!strcmp(option->name, "long-help")) {
			print_full_help(cmdline_options, "-");
		} else {
			print_short_help();
		}
	}

	fflush(stdout);
	return "";
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
		"base-session", 0, 0, MAXINT, 0,
		N_("ID of session (ELinks instance) which we want to clone.\n"
		"This is internal ELinks option, you don't want to use it.")),

	INIT_OPT_STRING("", N_("Set config dir to given string"),
		"confdir", 0, "",
		N_("Set the config dir to the given path. ELinks will read\n"
		"its config files and writes to it. If the path begins with\n"
		"a '/' its used as an absolute path. Else it is assumed to\n"
		"be relative to your HOME dir.")),

	INIT_OPT_STRING("", N_("Configuration file name"),
		"conffile", 0, "elinks.conf",
		N_("Name of the file with configuration, from which and to\n"
		"which all the configuration shall be written. It should be\n"
		"relative to confdir.")),

	INIT_OPT_ALIAS("", N_("MIME type to assume for documents"),
		"default-mime-type", 0, "mime.default_type",
		N_("Default MIME type to assume for documents of unknown type.")),

	INIT_OPT_BOOL("", N_("Write formatted version of given URL to stdout"),
		"dump", 0, 0,
		N_("Write a plain-text version of the given HTML document to\n"
		"stdout.")),

	INIT_OPT_ALIAS("", N_("Codepage to use with -dump"),
		"dump-charset", 0, "document.dump.codepage",
		N_("Codepage used in dump output.")),

	INIT_OPT_ALIAS("", N_("Width of document formatted with -dump"),
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

	INIT_OPT_COMMAND("", N_("Print help for configuration options"),
		"config-help", 0, printhelp_cmd,
		N_("Print help on configuration options and exit.")),

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

	INIT_OPT_INT("", N_("Connect to session ring with given ID"),
		"session-ring", 0, 0, MAXINT, 0,
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

	INIT_OPT_BOOL("", N_("Read document from stdin"),
		"stdin", 0, 0,
		N_("Open stdin as an HTML document - this is fully equivalent to:\n"
		" -eval 'set protocol.file.allow_special_files = 1' file:///dev/stdin\n"
		"Use whichever suits you more ;-). Note that reading document from\n"
		"stdin WORKS ONLY WHEN YOU USE -dump OR -source!! (I would like to\n"
		"know why you would use -source -stdin, though ;-)")),

	INIT_OPT_BOOL("", N_("Do not number links in dump output"),
		"no-numbering", 0, 0,
		N_("Prevents numbering of links (and showing their list at the end of\n"
		"the dumped document) in the -dump output; this was the default behaviour\n"
		"until 0.5pre12. Note that this really affects only --dump, nothing else.")),

	INIT_OPT_BOOL("", N_("Touch files in ~/.elinks when running with -no-connect/-session-ring"),
		"touch-files", 0, 0,
		N_("Set to 1 to have runtime state files (bookmarks, history, ...)\n"
		"changed even when -no-connect or -session-ring is used; has no\n"
		"effect if not used in connection with any of these options.")),

	INIT_OPT_COMMAND("", N_("Print version information and exit"),
		"version", 0, version_cmd,
		N_("Print ELinks version information and exit.")),

	NULL_OPTION_INFO,
};
