/* Version information */
/* $Id: version.c,v 1.1 2003/05/19 14:12:30 zas Exp $ */

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

unsigned char *
get_version()
{
	return (unsigned char *) "ELinks " VERSION_STRING;
}

unsigned char *
get_dyn_full_version(struct terminal *term)
{
	return straconcat(
		get_version(),
#ifdef DEBUG
		" ("
#ifdef __DATE__
		__DATE__
#endif
		" "
#ifdef __TIME__
		__TIME__
#endif
		")"
#endif
		"\n\n",
		_("Text WWW browser", term),
		"\n\n",
		_("Features:", term), " ",
#ifndef DEBUG
		_("Standard", term), ", ",
#else
		_("Debug", term), ", ",
#endif
#ifdef FASTMEM
		_("Fastmem", term), ", ",
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
		" bzip2",
#endif
		NULL
	);
}

/* This one is used to prevent usage of straconcat() at backtrace time. */
void
init_static_version()
{
	unsigned char *s = get_dyn_full_version((struct terminal *) NULL);

	memset(full_static_version, 0, sizeof(full_static_version));
	if (s) {
		int slen = strlen(s);

		if (slen) memcpy(full_static_version, s,
				 slen < sizeof(full_static_version)
				 ? slen
				 : sizeof(full_static_version) - 1);

		mem_free(s);
	}
}
