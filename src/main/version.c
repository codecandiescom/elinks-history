/* Version information */
/* $Id: version.c,v 1.24 2003/10/30 02:15:14 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "modules/module.h"
#include "modules/version.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


#define ELINKS_VERSION	("ELinks " VERSION_STRING)

static void
add_module_to_string(struct string *string, struct module *module,
		     struct terminal *term)
{
	int i;

	if (module->name) add_to_string(string, _(module->name, term));

	if (!module->submodules) return;

	add_to_string(string, " (");

	for (i = 0; module->submodules[i]; i++) {
		add_module_to_string(string, module->submodules[i], term);
		if (module->submodules[i + 1])
			add_to_string(string, ", ");
	}

	add_to_string(string, ")");
}

static void
add_modules_to_string(struct string *string, struct terminal *term)
{
	extern struct module *builtin_modules[];
	int i;

	for (i = 0; builtin_modules[i]; i++) {
		add_module_to_string(string, builtin_modules[i], term);
		if (builtin_modules[i + 1])
			add_to_string(string, ", ");
	}
}

/* @more will add more information especially for info box. */
unsigned char *
get_dyn_full_version(struct terminal *term, int more)
{
	static const unsigned char comma[] = ", ";
	struct string string;

	if (!init_string(&string)) return NULL;

	add_to_string(&string, ELINKS_VERSION);
	if (more) {
 #if defined(DEBUG) && defined(__DATE__) && defined(__TIME__)
		add_to_string(&string, " (" __DATE__ " " __TIME__ ")");
 #endif
		add_to_string(&string, "\n\n");
		add_to_string(&string, _("Text WWW browser", term));
	}

	string_concat(&string,
		"\n\n",
		_("Features:", term), " ",
#ifndef DEBUG
		_("Standard", term),
#else
		_("Debug", term),
#endif
#ifdef FASTMEM
		comma, _("Fastmem", term),
#endif
#ifdef USE_OWN_LIBC
		comma, _("Own Libc Routines", term),
#endif
#ifndef BACKTRACE
		comma, _("No Backtrace", term),
#endif
#ifdef IPV6
		comma, "IPv6",
#endif
#ifdef HAVE_ZLIB_H
		comma, "gzip",
#endif
#ifdef HAVE_BZLIB_H
		comma, "bzip2",
#endif
#ifndef USE_MOUSE
		comma, _("No mouse", term),
#endif
		comma,
		NULL
	);

	add_modules_to_string(&string, term);

	return string.source;
}

/* This one is used to prevent usage of straconcat() at backtrace time. */
void
init_static_version(void)
{
	unsigned char *s = get_dyn_full_version((struct terminal *) NULL, 0);

	if (s) {
		safe_strncpy(full_static_version, s, sizeof(full_static_version));
		mem_free(s);
	}
}
