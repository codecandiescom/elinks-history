/* Version information */
/* $Id: version.c,v 1.18 2003/10/13 21:49:16 jonas Exp $ */

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
#include "terminal/terminal.h"
#ifdef LEAK_DEBUG
#include "util/memdebug.h"
#endif
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/version.h"


unsigned char full_static_version[1024];

unsigned char *
get_version(void)
{
	return (unsigned char *) "ELinks " VERSION_STRING;
}

/* @more will add more information especially for info box. */
unsigned char *
get_dyn_full_version(struct terminal *term, int more)
{
	static const unsigned char comma[] = ", ";

	return straconcat(
		get_version(),
#if defined(DEBUG) && defined(__DATE__) && defined(__TIME__)
		(unsigned char *) (more ? " (" __DATE__ " " __TIME__ ")" : ""),
#endif
		(unsigned char *) (more ? "\n\n": ""),
		more ? _("Text WWW browser", term) : (unsigned char *) "",
		(unsigned char *) (more ? "\n\n" : "\n"),
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
#ifdef HAVE_SSL
		comma, _("SSL", term), " ",
#ifdef HAVE_OPENSSL
		"(OpenSSL)",
#elif defined(HAVE_GNUTLS)
		"(GnuTLS)",
#endif
#endif
#ifdef HAVE_LUA
		comma, "Lua",
#endif
#ifdef HAVE_GUILE
		comma, "Guile",
#endif
#ifdef IPV6
		comma, "IPv6",
#endif
#ifdef BOOKMARKS
		comma, _("Bookmarks", term),
#endif
#ifdef COOKIES
		comma, _("Cookies", term),
#endif
#ifdef GLOBHIST
		comma, _("Global History", term),
#endif
#ifdef USE_LEDS
		comma, _("LED indicators", term),
#endif
#ifdef HAVE_ZLIB_H
		comma, "gzip",
#endif
#ifdef HAVE_BZLIB_H
		comma, "bzip2",
#endif
#ifdef MAILCAP
		comma, _("Mailcap", term),
#endif
#ifdef MIMETYPES
		comma, _("Mimetypes files", term),
#endif
#ifdef FORMS_MEMORY
		comma, _("Forms memory", term),
#endif
#ifndef USE_MOUSE
		comma, _("No mouse", term),
#endif
		NULL
	);
}

/* This one is used to prevent usage of straconcat() at backtrace time. */
void
init_static_version(void)
{
	unsigned char *s = get_dyn_full_version((struct terminal *) NULL, 0);

	full_static_version[0] = '\0';
	if (s) {
		safe_strncpy(full_static_version, s, sizeof(full_static_version));
		mem_free(s);
	}
}
