/* Option system based mime backend */
/* $Id: default.c,v 1.1 2003/05/06 19:54:25 jonas Exp $ */

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
#include "protocol/url.h"
#include "util/memory.h"
#include "util/string.h"

#define BACKEND_NAME	"optionsystem"

unsigned char *
get_content_type_default(unsigned char *url)
{	
	struct option *opt_tree;
	struct option *opt;

	opt_tree = get_opt_rec_real(&root_options, "mime.extension");

	foreach (opt, *((struct list_head *) opt_tree->ptr)) {
		/* strrcmp */
		int i, j;

		/* Match the longest possible part of URL.. */

		for (i = strlen(url) - 1, j = strlen(opt->name) - 1;
			i >= 0 && j >= 0
			&& url[i] == (opt->name[j] == '*' ? '.'
				: opt->name[j]);
			i--, j--)
			/* */ ;

		/* If we matched whole extension and it is really an
		 * extension.. */
		if (j < 0 && i >= 0 && url[i] == '.') {
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

	class = stracpy(type);
	if (!class) return NULL;
	rmdots(class);

	id = strchr(class, '/');
	if (!id) {
		mem_free(class);
		return NULL;
	}
	*(id++) = '\0';
	rmdots(id);

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
	unsigned char *name;

	name = get_mime_handler_name(type, have_x);
	if (!name) return NULL;

	opt_tree = get_opt_rec_real(&root_options, name);
	mem_free(name);

	if (opt_tree) {
		struct mime_handler *handler;
		unsigned char *mt = get_mime_type_name(type);

		/* Try to find some description to assing to @name */
		if (mt) {
			struct option *opt;
			
			opt = get_opt_rec_real(&root_options, mt);
			mem_free(mt);

			if (opt)
				name = opt->ptr;
		}

		handler = mem_alloc(sizeof(struct mime_handler));
		if (!handler) return NULL;

		if (get_opt_bool_tree(opt_tree, "block"))
			handler->flags |= MIME_BLOCK;

		if (get_opt_bool_tree(opt_tree, "ask"))
			handler->flags |= MIME_ASK;

		handler->program = stracpy(get_opt_str_tree(opt_tree, "program"));
		handler->description = name ? name : NULL;
		handler->backend_name = BACKEND_NAME;

		return handler;
	}

	return NULL;
}


/* Setup the exported backend */
struct mime_backend default_mime_backend = {
	/* init: */		NULL,
	/* done: */		NULL,
	/* get_content_type: */	get_content_type_default,
	/* get_mime_handler: */	get_mime_handler_default,
};
