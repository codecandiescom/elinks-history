/* Command line processing */
/* $Id: cmdline.c,v 1.4 2002/05/26 17:56:21 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "links.h"

#include "config/cmdline.h"
#include "config/options.h"
#include "config/opttypes.h"


/* TODO: This ought to be rewritten - we want special tree for commandline
 * options as "aliases" there. */
unsigned char *
_parse_options(int argc, unsigned char *argv[], struct list_head *opt)
{
	unsigned char *location = NULL;

	while (argc) {
		argv++, argc--;

		if (argv[-1][0] == '-') {
			struct option *option;
			unsigned char *argname = &argv[-1][1];
			unsigned char *oname = opt_name(argname);

			/* Treat --foo same as -foo. */
			if (argname[0] == '-') argname++;

			option = get_opt_rec(opt, argname);
			if (!option && oname)
				option = get_opt_rec(opt, oname);

			mem_free(oname);

			if (!option)
				continue;

			if (option_types[option->type].cmdline
			    && 1 /*option->flags & OPT_CMDLINE*/) {
				unsigned char *err;

				err = option_types[option->type].cmdline(option, &argv, &argc);

				if (err) {
					if (err[0])
						fprintf(stderr, "Error parsing option %s: %s\n", argv[-1], err);

					return NULL;
				}

				goto found;
			}

			goto unknown_option;

		} else if (!location) {
			location = argv[-1];

		} else {
unknown_option:		fprintf(stderr, "Unknown option %s\n", argv[-1]);

			return NULL;
		}

found:
	}

	return location ? location : (unsigned char *) "";
}

unsigned char *
parse_options(int argc, unsigned char *argv[])
{
	return _parse_options(argc, argv, root_options);
}
