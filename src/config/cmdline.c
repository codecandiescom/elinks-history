/* Command line processing */
/* $Id: cmdline.c,v 1.16 2003/06/08 22:11:46 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "elinks.h"

#include "config/cmdline.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/gettext/libintl.h"
#include "util/memory.h"


/* TODO: This ought to be rewritten - we want special tree for commandline
 * options as "aliases" there. */
static unsigned char *
_parse_options(int argc, unsigned char *argv[], struct option *opt)
{
	unsigned char *location = NULL;

	while (argc) {
		argv++, argc--;

		if (argv[-1][0] == '-') {
			struct option *option;
			unsigned char *argname = &argv[-1][1];
			unsigned char *oname = stracpy(argname);

			if (!oname) continue;

			/* Treat --foo same as -foo. */
			if (argname[0] == '-') argname++;

			option = get_opt_rec(opt, argname);
			if (!option && oname)
				option = get_opt_rec(opt, oname);

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
						error(gettext("Cannot parse option %s: %s"), argv[-1], err);

					return NULL;
				}
			} else {
				goto unknown_option;
			}

		} else if (!location) {
			location = argv[-1];

		} else {
unknown_option:
			error(gettext("Unknown option %s"), argv[-1]);
			return NULL;
		}
	}

	return location ? location : (unsigned char *) "";
}

unsigned char *
parse_options(int argc, unsigned char *argv[])
{
	return _parse_options(argc, argv, &cmdline_options);
}
