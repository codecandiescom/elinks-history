/* Version information */
/* $Id: version.c,v 1.7 2003/06/05 14:38:16 zas Exp $ */

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
#include "version.h"

unsigned char full_static_version[1024];

unsigned char *
get_version(void)
{
	return (unsigned char *) "ELinks " VERSION_STRING;
}

/* @more will add longer informations especially for info box. */
unsigned char *
get_dyn_full_version(struct terminal *term, int more)
{
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
		_("Standard", term), ", ",
#else
		_("Debug", term), ", ",
#endif
#ifdef FASTMEM
		_("Fastmem", term), ", ",
#endif
#ifndef BACKTRACE
		_("No backtrace", term), ", ",
#endif
#ifdef HAVE_SSL
		_("SSL", term),
#ifdef HAVE_OPENSSL
		"(OpenSSL)",
#elif defined(HAVE_GNUTLS)
		"(GNUTLS)",
#endif
		 ", ",
#endif
#ifdef HAVE_LUA
		"Lua", ", ",
#endif
#ifdef IPV6
		"IPv6", ", ",
#endif
#ifdef BOOKMARKS
		_("Bookmarks", term), ", ",
#endif
#ifdef COOKIES
		_("Cookies", term), ", ",
#endif
#ifdef GLOBHIST
		_("GlobHist", term), ", ",
#endif
#ifdef HAVE_ZLIB_H
		"gzip" ", ",
#endif
#ifdef HAVE_BZLIB_H
		"bzip2",
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
