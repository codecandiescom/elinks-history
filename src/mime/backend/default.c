/* Option system based mime backend */
/* $Id: default.c,v 1.15 2003/07/08 12:18:41 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "terminal/terminal.h"
#include "mime/backend/common.h"
#include "mime/backend/default.h"
#include "mime/mime.h"
#include "osdep/os_dep.h"		/* For get_system_str() */
#include "util/memory.h"
#include "util/string.h"

#define BACKEND_NAME	"optionsystem"

static unsigned char *
get_content_type_default(unsigned char *extension)
{
	struct option *opt_tree;
	struct option *opt;
	unsigned char *extend = extension + strlen(extension) - 1;

	if (extend < extension)
		return NULL;

	opt_tree = get_opt_rec_real(&root_options, "mime.extension");

	foreach (opt, *((struct list_head *) opt_tree->ptr)) {
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
		if (namepos < opt->name) {
			return stracpy(opt->ptr);
		}
	}

	return NULL;
}

static inline void
rmdots(unsigned char *tok)
{
	while (*tok) {
		if (*tok == '.') *tok = '*';
		tok++;
	}
}

static unsigned char *
get_mime_type_name(unsigned char *type)
{
	unsigned char *class, *id;
	unsigned char *name;

	class = encode_option_name(type);
	if (!class) return NULL;

	id = strchr(class, '/');
	if (!id) {
		mem_free(class);
		return NULL;
	}
	*(id++) = '\0';

	name = straconcat("mime.type.", class, ".", id, NULL);
	mem_free(class);

	return name;
}

static inline unsigned char *
get_mime_handler_name(unsigned char *type, int xwin)
{
	struct option *opt;
	unsigned char *name = get_mime_type_name(type);
	unsigned char *system_str;

	if (!name) return NULL;

	opt = get_opt_rec_real(&root_options, name);
	mem_free(name);
	if (!opt) return NULL;

	system_str = get_system_str(xwin);
	if (!system_str) return NULL;

	name = straconcat("mime.handler.", (unsigned char *) opt->ptr,
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

	opt_tree = get_opt_rec_real(&root_options, handler_name);
	mem_free(handler_name);

	if (opt_tree) {
		struct mime_handler *handler;
		unsigned char *desc = "";
		unsigned char *mt = get_mime_type_name(type);

		/* Try to find some description to assing to @name */
		if (mt) {
			struct option *opt;

			opt = get_opt_rec_real(&root_options, mt);
			mem_free(mt);

			if (opt)
				desc = opt->ptr;
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
	NULL_LIST_HEAD,
	/* name: */		BACKEND_NAME,
	/* init: */		NULL,
	/* done: */		NULL,
	/* get_content_type: */	get_content_type_default,
	/* get_mime_handler: */	get_mime_handler_default,
};
