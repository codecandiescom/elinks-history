/* Option system based mime backend */
/* $Id: default.c,v 1.22 2003/10/22 19:24:46 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "mime/backend/common.h"
#include "mime/backend/default.h"
#include "mime/mime.h"
#include "osdep/os_dep.h"		/* For get_system_str() */
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"

#define BACKEND_NAME	"optionsystem"

static unsigned char *
get_content_type_default(unsigned char *extension)
{
	struct option *opt_tree;
	struct option *opt;
	unsigned char *extend = extension + strlen(extension) - 1;

	if (extend < extension)	return NULL;

	opt_tree = get_opt_rec_real(config_options, "mime.extension");

	foreach (opt, *opt_tree->value.tree) {
		unsigned char *namepos = opt->name + strlen(opt->name) - 1;
		unsigned char *extpos = extend;

		/* Match the longest possible part of URL.. */

#define star2dot(achar)	((achar) == '*' ? '.' : (achar))

		while (extension <= extpos && opt->name <= namepos
		       && *extpos == star2dot(*namepos)) {
			extpos--;
			namepos--;
		}

#undef star2dot

		/* If we matched whole extension and it is really an
		 * extension.. */
		if ((namepos < opt->name)
		    && ((extpos < extension) || (*extpos == '.')))
			return stracpy(opt->value.string);
	}

	return NULL;
}

static unsigned char *
get_mime_type_name(unsigned char *type)
{
	struct string name;
	int oldlength;

	if (!init_string(&name)) return NULL;

	add_to_string(&name, "mime.type.");
	oldlength = name.length;
	if (add_optname_to_string(&name, type, strlen(type))) {
		unsigned char *pos = name.source + oldlength;

		/* Search for end of the base type. */
		pos = strchr(pos, '/');
		if (pos) {
			*pos = '.';
			return name.source;
		}
	}

	done_string(&name);
	return NULL;
}

static inline unsigned char *
get_mime_handler_name(unsigned char *type, int xwin)
{
	struct option *opt;
	unsigned char *name = get_mime_type_name(type);
	unsigned char *system_str;

	if (!name) return NULL;

	opt = get_opt_rec_real(config_options, name);
	mem_free(name);
	if (!opt) return NULL;

	system_str = get_system_str(xwin);
	if (!system_str) return NULL;

	name = straconcat("mime.handler.", opt->value.string,
			  ".", system_str, NULL);
	mem_free(system_str);

	return name;
}

static struct mime_handler *
get_mime_handler_default(unsigned char *type, int have_x)
{
	struct option *opt_tree;
	unsigned char *handler_name = get_mime_handler_name(type, have_x);

	if (!handler_name) return NULL;

	opt_tree = get_opt_rec_real(config_options, handler_name);
	mem_free(handler_name);

	if (opt_tree) {
		struct mime_handler *handler;
		unsigned char *desc = "";
		unsigned char *mt = get_mime_type_name(type);

		/* Try to find some description to assing to @name */
		if (mt) {
			struct option *opt;

			opt = get_opt_rec_real(config_options, mt);
			mem_free(mt);

			if (opt) desc = opt->value.string;
		}

		handler = mem_alloc(sizeof(struct mime_handler));
		if (!handler) return NULL;

		handler->block = get_opt_bool_tree(opt_tree, "block");
		handler->ask = get_opt_bool_tree(opt_tree, "ask");
		handler->program = stracpy(get_opt_str_tree(opt_tree, "program"));
		handler->description = desc;
		handler->backend_name = BACKEND_NAME;

		return handler;
	}

	return NULL;
}


/* Setup the exported backend */
struct mime_backend default_mime_backend = {
	/* name: */		BACKEND_NAME,
	/* init: */		NULL,
	/* done: */		NULL,
	/* get_content_type: */	get_content_type_default,
	/* get_mime_handler: */	get_mime_handler_default,
};
