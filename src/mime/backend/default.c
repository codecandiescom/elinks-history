/* Option system based mime backend */
/* $Id: default.c,v 1.30 2003/10/27 23:25:35 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "mime/backend/common.h"
#include "mime/backend/default.h"
#include "mime/mime.h"
#include "osdep/osdep.h"		/* For get_system_str() */
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


static struct option_info default_mime_options[] = {
	INIT_OPT_TREE("mime", N_("MIME type associations"),
		"type", OPT_AUTOCREATE,
		N_("Handler <-> MIME type association. The first sub-tree is the MIME\n"
		"class while the second sub-tree is the MIME type (ie. image/gif\n"
		"handler will reside at mime.type.image.gif). Each MIME type option\n"
		"should contain (case-sensitive) name of the MIME handler (its\n"
		"properties are stored at mime.handler.<name>).")),

	INIT_OPT_TREE("mime.type", NULL,
		"_template_", OPT_AUTOCREATE,
		N_("Handler matching this MIME-type class ('*' is used here in place\n"
		"of '.').")),

	INIT_OPT_STRING("mime.type._template_", NULL,
		"_template_", 0, "",
		N_("Handler matching this MIME-type name ('*' is used here in place\n"
		"of '.').")),


	INIT_OPT_TREE("mime", N_("File type handlers"),
		"handler", OPT_AUTOCREATE,
		N_("Handler for certain MIME types (as specified in mime.type.*).\n"
		"Each handler usually serves family of MIME types (ie. images).")),

	INIT_OPT_TREE("mime.handler", NULL,
		"_template_", OPT_AUTOCREATE,
		N_("Description of this handler.")),

	INIT_OPT_TREE("mime.handler._template_", NULL,
		"_template_", 0,
		N_("System-specific handler description (ie. unix, unix-xwin, ...).")),

	INIT_OPT_BOOL("mime.handler._template_._template_", N_("Ask before opening"),
		"ask", 0, 1,
		N_("Ask before opening.")),

	INIT_OPT_BOOL("mime.handler._template_._template_", N_("Block terminal"),
		"block", 0, 1,
		N_("Block the terminal when the handler is running.")),

	INIT_OPT_STRING("mime.handler._template_._template_", N_("Program"),
		"program", 0, "",
		N_("External viewer for this file type. '%' in this string will be\n"
		"substituted by a file name.")),


	INIT_OPT_TREE("mime", N_("File extension associations"),
		"extension", OPT_AUTOCREATE,
		N_("Extension <-> MIME type association.")),

	INIT_OPT_STRING("mime.extension", NULL,
		"_template_", 0, "",
		N_("MIME-type matching this file extension ('*' is used here in place\n"
		"of '.').")),

#define INIT_OPT_MIME_EXTENSION(extension, type) \
	INIT_OPT_STRING("mime.extension", NULL, extension, 0, type, NULL)

	INIT_OPT_MIME_EXTENSION("gif",	"image/gif"),
	INIT_OPT_MIME_EXTENSION("jpg",	"image/jpg"),
	INIT_OPT_MIME_EXTENSION("jpeg",	"image/jpeg"),
	INIT_OPT_MIME_EXTENSION("png",	"image/png"),
	INIT_OPT_MIME_EXTENSION("txt",	"text/plain"),
	INIT_OPT_MIME_EXTENSION("htm",	"text/html"),
	INIT_OPT_MIME_EXTENSION("html",	"text/html"),

	NULL_OPTION_INFO,
};

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
		handler->backend_name = default_mime_module.name;

		return handler;
	}

	return NULL;
}


struct mime_backend default_mime_backend = {
	/* get_content_type: */	get_content_type_default,
	/* get_mime_handler: */	get_mime_handler_default,
};

struct module default_mime_module = struct_module(
	/* name: */		N_("Option system"),
	/* options: */		default_mime_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
