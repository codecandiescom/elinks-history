/* URL parser and translator; implementation of RFC 2396. */
/* $Id: url.c,v 1.90 2003/07/14 18:48:59 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h> /* OS/2 needs this after sys/types.h */

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "protocol/url.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* Returns -1 if url is unknown or invalid. */
static int
parse_url(unsigned char *url, int *prlen,
          unsigned char **user, int *uslen,
          unsigned char **pass, int *palen,
          unsigned char **host, int *holen,
          unsigned char **port, int *polen,
          unsigned char **data, int *dalen,
          unsigned char **post)
{
	struct uri uri;
	int uricode = parse_uri(&uri, url);

#if 0
	/* Temporary debug code */
	debug("parse_uri returned [%d]", uricode);
	if (uricode > 0) {
		debug("protocollen = %d", uri.protocollen);
		debug("user        = %s", uri.user);
		debug("userlen     = %d", uri.userlen);
		debug("password    = %s", uri.password);
		debug("passwordlen = %d", uri.passwordlen);
		debug("host        = %s", uri.host);
		debug("hostlen     = %d", uri.hostlen);
		debug("port        = %s", uri.port);
		debug("portlen     = %d", uri.portlen);
		debug("data        = %s", uri.data);
		debug("datalen     = %d", uri.datalen);
		debug("post        = %s", uri.post);
	}
#endif
	if (prlen) *prlen = uri.protocollen;
	if (user)  *user  = uri.user;
	if (uslen) *uslen = uri.userlen;
	if (pass)  *pass  = uri.password;
	if (palen) *palen = uri.passwordlen;
	if (host)  *host  = uri.host;
	if (holen) *holen = uri.hostlen;
	if (port)  *port  = uri.port;
	if (polen) *polen = uri.portlen;
	if (data)  *data  = uri.data;
	if (dalen) *dalen = uri.datalen;
	if (post)  *post  = uri.post;
	return (uricode > 0 ? 0 : -1);
}

static unsigned char *
get_url_data(unsigned char *url)
{
	unsigned char *d;

	if (parse_url(url, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      &d, NULL,
		      NULL)) return NULL;

	return d;
}



#define dsep(x) (lo ? dir_sep(x) : (x) == '/')

static void
translate_directories(unsigned char *url)
{
	unsigned char *url_data = get_url_data(url);
	unsigned char *src, *dest;
	int lo = !strncasecmp(url, "file://", 7); /* dsep() *hint* *hint* */

	if (!url_data || url_data == url/* || *--url_data != '/'*/) return;
	if (!dsep(*url_data)) url_data--;
	src = url_data;
	dest = url_data;

	do {
		/* TODO: Rewrite this parser in sane way, gotos are ugly ;). */
repeat:
		if (end_of_dir(src[0])) {
			/* URL data contains no more path. */
			memmove(dest, src, strlen(src) + 1);
			return;
		}

		/* If the following pieces are the LAST parts of URL, we remove
		 * them as well. See RFC 1808 for details. */

		if (dsep(src[0]) && src[1] == '.'
		    && (!src[2] || dsep(src[2]))) {

			/* /./ - strip that.. */

			if (src == url_data && (!src[2] || !src[3])) {
				/* ..if this is not the only URL (why?). */
				goto proceed;
			}

			src += 2;
			goto repeat;
		}

		if (dsep(src[0]) && src[1] == '.' && src[2] == '.'
		    && (!src[3] || dsep(src[3]))) {
			unsigned char *orig_dest = dest;

			/* /../ - strip that and preceding element. */

			while (dest > url_data) {
				dest--;
				if (dsep(*dest)) {
					if (dest + 3 == orig_dest
					    && dest[1] == '.'
					    && dest[2] == '.') {
						dest = orig_dest;
						goto proceed;
					}
					break;
				}
			}

			src += 3;
			goto repeat;
		}

proceed: ;
	} while ((*dest++ = *src++));
}


static void
insert_wd(unsigned char **up, unsigned char *cwd)
{
	unsigned char *url = *up;
	int cwdlen;

	if (!url || !cwd || !*cwd
	    || strncasecmp(url, "file://", 7))
		return;
	if (!strncasecmp(url + 7, "localhost/", 10)) {
		/* Remove localhost from the URL to make (not only) the
		 * file:// handler happy. */
		memmove(url + 7, url + 16, strlen(&url[16]) + 1);
	}
	if (dir_sep(url[7]))
		return;
#ifdef DOS_FS
	if (upcase(url[7]) >= 'A' && upcase(url[7]) <= 'Z' && url[8] == ':' && dir_sep(url[9])) return;
#endif

	cwdlen = strlen(cwd);
	url = mem_alloc(strlen(*up) + cwdlen + 2);
	if (!url) return;

	memcpy(url, *up, 7);
	strcpy(url + 7, cwd);

	if (!dir_sep(cwd[cwdlen - 1])) strcat(url, "/");

	strcat(url, *up + 7);
	mem_free(*up);
	*up = url;
}


unsigned char *
join_urls(unsigned char *base, unsigned char *rel)
{
	unsigned char *p, *n, *path;
	int lo = !strncasecmp(base, "file://", 7); /* dsep() *hint* *hint* */
	int add_slash = 0;
	struct uri uri;

	/* See RFC 1808 */
	/* TODO: Support for ';' ? (see the RFC) --pasky */

	if (rel[0] == '#') {
		n = stracpy(base);
		if (!n) return NULL;

		for (p = n; *p && *p != POST_CHAR && *p != '#'; p++);
		*p = '\0';
		add_to_strn(&n, rel);
		translate_directories(n);

		return n;
	} else if (rel[0] == '?') {
		n = stracpy(base);
		if (!n) return NULL;

		for (p = n; *p && *p != POST_CHAR && *p != '?' && *p != '#'; p++);
		*p = '\0';
		add_to_strn(&n, rel);
		translate_directories(n);

		return n;
	} else if (rel[0] == '/' && rel[1] == '/') {
		unsigned char *s = strstr(base, "//");

		if (!s) {
			internal("bad base url: %s", base);
			return NULL;
		}

		n = memacpy(base, s - base);
		add_to_strn(&n, rel);
		return n;
	}

	if (!strncasecmp("proxy://", rel, 8)) goto prx;

	if (parse_uri(&uri, rel)) {
		n = stracpy(rel);
		if (n) translate_directories(n);

		return n;
	}

	n = stracpy(rel);
	if (n) {
		while (n[0] && n[strlen(n) - 1] <= ' ') n[strlen(n) - 1] = 0;
		add_to_strn(&n, "/");

		if (parse_uri(&uri, n)) {
			translate_directories(n);
			return n;
		}

		mem_free(n);
	}

prx:
	if (!parse_uri(&uri, base) || !uri.data) {
		internal("bad base url");
		return NULL;
	}
	path = uri.data;

	/* Either is path blank, but we've slash char before, or path is not
	 * blank, but doesn't start by a slash (if we'd just stay along with
	 * dsep(path[-1]) w/o all the surrounding crap, it should be enough,
	 * but I'm not sure and I don't want to break anything --pasky). */
	if ((!*path && dsep(path[-1])) || (*path && !dsep(*path))) {
		/* We skip first char of URL ('/') in parse_url() (ARGH). This
		 * is reason of all this bug-bearing magic.. */
		path--;
	}

	if (!dsep(rel[0])) {
		unsigned char *path_end;

		/* The URL is relative. */

		if (!*path) {
			/* There's no path in the URL, but we're going to add
			 * something there, and the something doesn't start by
			 * a slash. So we need to insert a slash after the base
			 * URL. Clever, eh? ;) */
			add_slash = 1;
		}

		for (path_end = path; *path_end; path_end++) {
			if (end_of_dir(*path_end)) break;
			/* Modify the path pointer, so that it'll always point
			 * above the last '/' in the URL; later, we'll copy the
			 * URL only _TO_ this point, and anything after last
			 * slash will be substituted by 'rel'. */
			if (dsep(*path_end)) path = path_end + 1;
		}
	}

	n = mem_alloc(path - base + strlen(rel) + add_slash + 1);
	if (!n) return NULL;

	memcpy(n, base, path - base);
	if (add_slash) n[path - base] = '/';
	strcpy(n + (path - base) + add_slash, rel);

	translate_directories(n);
	return n;
}


unsigned char *
translate_url(unsigned char *url, unsigned char *cwd)
{
	unsigned char *ch;
	unsigned char *newurl;
	struct uri uri;

	/* Strip starting spaces */
	while (*url == ' ') url++;

	/* XXX: Why?! */
	if (!strncasecmp("proxy://", url, 8)) goto proxy;

	/* Ordinary parse */
	if (parse_uri(&uri, url)) {
		newurl = stracpy(url);
		if (newurl) {
			insert_wd(&newurl, cwd);
			translate_directories(newurl);
		}

		return newurl;
	}

	/* Try to add slash to end */
	if (strstr(url, "//") && (newurl = stracpy(url))) {
		add_to_strn(&newurl, "/");
		if (parse_uri(&uri, newurl)) {
			insert_wd(&newurl, cwd);
			translate_directories(newurl);

			return newurl;
		}
		mem_free(newurl);
	}

proxy:
	/* No protocol name */
	ch = url + strcspn(url, ".:/@");
#ifdef IPV6
	if (*ch != ':' || *url == '[' || url[strcspn(url, "/@")] == '@') {
#else
	if (*ch != ':' || url[strcspn(url, "/@")] == '@') {
#endif
		unsigned char *prefix = "file://";
		int not_file = 0;

		/* Yes, it would be simpler to make test for IPv6 address first,
		 * but it would result in confusing mix of ifdefs ;-). */

		if (*ch == '@' || (*ch == ':' && *url != '[')
		    || !strncasecmp(url, "ftp.", 4)) {
			/* Contains user/password/ftp-hostname */
			prefix = "ftp://";
			not_file = 1;

#ifdef IPV6
		} else if (*url == '[' && *ch == ':') {
			/* Candidate for IPv6 address */
			unsigned char *bracket2, *colon2;

			ch++;
			bracket2 = strchr(ch, ']');
			colon2 = strchr(ch, ':');
			if (bracket2 && colon2 && bracket2 > colon2)
				goto http;
#endif

		} else if (*url != '.' && *ch == '.') {
			/* Contains domain name? */
			unsigned char *host_end, *domain;
			int i;

			/* Process the hostname */
			for (domain = ch + 1;
			     *(host_end = domain + strcspn(domain, ".:/")) == '.';
			     domain = host_end + 1);

			/* It's IP? */
			for (i = 0; i < host_end - domain; i++)
				if (domain[i] >= '0' && domain[i] <= '9')
					goto http;

			/* FIXME: Following is completely braindead.
			 * TODO: Remove it. We should rather first try file:// and
			 * then http://, if failed. But this will require wider
			 * modifications. :| --pasky */

			/* It's two-letter TLD? */
			if (host_end - domain == 2) {
http:				prefix = "http://";
				not_file = 1;

			} else {
				/* See above the braindead FIXME :^). */
				unsigned char *tld[] = { "com", "edu", "net",
							 "org", "gov", "mil",
							 "int", "biz", "arpa",
							 "aero", "coop",
							 "info", "museum",
							 "name", "pro", NULL };

				for (i = 0; tld[i]; i++)
					if (host_end - domain == strlen(tld[i])
					    && !strncasecmp(tld[i], domain,
						        host_end - domain))
						goto http;
			}
		}

		newurl = stracpy(prefix);
		if (!newurl) return NULL;
		add_to_strn(&newurl, url);
		if (not_file && !strchr(url, '/')) add_to_strn(&newurl, "/");

		if (parse_uri(&uri, newurl)) {
			insert_wd(&newurl, cwd);
			translate_directories(newurl);

			return newurl;
		}

		mem_free(newurl);
		return NULL;
	}

	newurl = memacpy(url, ch - url + 1);
	if (!newurl) return NULL;

	/* Try prefix:some.url -> prefix://some.url.. */
	if (strncmp(ch + 1, "//", 2)) {
		add_to_strn(&newurl, "//");
		add_to_strn(&newurl, ch + 1);
		if (!parse_uri(&uri, newurl)) {
			insert_wd(&newurl, cwd);
			translate_directories(newurl);

			return newurl;
		}
	}

	/* ..and with slash */
	add_to_strn(&newurl, "/");
	if (parse_uri(&uri, newurl)) {
		insert_wd(&newurl, cwd);
		translate_directories(newurl);

		return newurl;
	}

	mem_free(newurl);
	return NULL;
}


unsigned char *
extract_position(unsigned char *url)
{
	unsigned char *uu, *r;
	unsigned char *u = strchr(url, POST_CHAR);

	if (!u) u = url + strlen(url);
	uu = u;
	while (--uu >= url && *uu != '#');

	if (uu < url) return NULL;

	r = mem_alloc(u - uu);
	if (!r) return NULL;

	memcpy(r, uu + 1, u - uu - 1);
	r[u - uu - 1] = 0;

	memmove(uu, u, strlen(u) + 1);

	return r;
}

unsigned char *
extract_proxy(unsigned char *uristring)
{
	unsigned char *proxy_end;

	if (strlen(uristring) < 8 || strncasecmp(uristring, "proxy://", 8))
		return uristring;

	proxy_end = strchr(uristring + 8, '/');
	if (!proxy_end) return uristring;

	return proxy_end + 1;
}

void
get_filename_from_url(unsigned char *url, unsigned char **s, int *l)
{
	int lo = !strncasecmp(url, "file://", 7); /* dsep() *hint* *hint* */
	unsigned char *uu = get_url_data(url);

	if (uu) url = uu;
	*s = url;
	while (*url && !end_of_dir(*url)) {
		if (dsep(*url)) *s = url + 1;
		url++;
	}
	*l = url - *s;
}

unsigned char *
get_extension_from_url(unsigned char *url)
{
	int lo = !strncasecmp(url, "file://", 7); /* dsep() *hint* *hint* */
	unsigned char *extension = NULL;
	int afterslash = 1;

 	for (; *url && !end_of_dir(*url); url++) {
		if (!afterslash && *url == '.' && !extension) {
			extension = url + 1;
		} else if (dsep(*url)) {
			extension = NULL;
			afterslash = 1;
		} else {
			afterslash = 0;
		}
	}

	if (extension && extension < url)
		return memacpy(extension, url - extension);

	return NULL;
}

#undef dsep
