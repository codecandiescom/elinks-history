/* URL parser and translator; implementation of RFC 2396. */
/* $Id: url.c,v 1.73 2003/06/26 20:19:50 jonas Exp $ */

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
	unsigned char *prefix_end, *host_end;
#ifdef IPV6
	unsigned char *lbracket, *rbracket;
#endif
	int scheme;

	if (prlen) *prlen = 0;
	if (user) *user = NULL;
	if (uslen) *uslen = 0;
	if (pass) *pass = NULL;
	if (palen) *palen = 0;
	if (host) *host = NULL;
	if (holen) *holen = 0;
	if (port) *port = NULL;
	if (polen) *polen = 0;
	if (data) *data = NULL;
	if (dalen) *dalen = 0;
	if (post) *post = NULL;

#ifdef DEBUG
	if (!url) internal("No url to parse.");
#endif

	if (!*url) return -1; /* Empty url. */

	/* Isolate prefix */

	prefix_end = strchr(url, ':');
	if (!prefix_end) return -1;

	if (prlen) *prlen = prefix_end - url;

	/* Get protocol */

	scheme = check_protocol(url, prefix_end - url);
	if (scheme == SCHEME_UNKNOWN) return -1;

	prefix_end++; /* ':' */

	/* Skip slashes */

	if (prefix_end[0] == '/' && prefix_end[1] == '/')
		prefix_end += 2;
	else if (get_protocol_need_slashes(scheme))
		return -1;

	if (get_protocol_free_syntax(scheme)) {
		if (data) *data = prefix_end;
		if (dalen) *dalen = strlen(prefix_end);
		return 0;
	}

	/* Isolate host */

#ifdef IPV6
	/* Get brackets enclosing IPv6 address */
	lbracket = strchr(prefix_end, '[');
	if (lbracket) {
		rbracket = strchr(lbracket, ']');
		/* [address] is handled only inside of hostname part (surprisingly). */
		if (rbracket && prefix_end + strcspn(prefix_end, "/") < rbracket)
			lbracket = rbracket = NULL;
	} else {
		rbracket = NULL;
	}
#endif

	/* Possibly skip auth part */
	host_end = prefix_end + strcspn(prefix_end, "@");

	if (prefix_end + strcspn(prefix_end, "/") > host_end
	    && *host_end) { /* we have auth info here */
		if (user || uslen || pass || palen) {
			unsigned char *user_end = strchr(prefix_end, ':');

			if (!user_end || user_end > host_end) {
				if (user) *user = prefix_end;
				if (uslen) *uslen = host_end - prefix_end;
			} else {
				if (user) *user = prefix_end;
				if (uslen) *uslen = user_end - prefix_end;
				if (pass) *pass = user_end + 1;
				if (palen) *palen = host_end - user_end - 1;
			}
		}
		prefix_end = host_end + 1;
	}

#ifdef IPV6
	if (rbracket)
		host_end = rbracket + strcspn(rbracket, ":/");
	else
#endif
		host_end = prefix_end + strcspn(prefix_end, ":/");

	if (!*host_end && get_protocol_need_slash_after_host(scheme))
		return -1;

	if (host || holen) { /* Only enter if needed. */
#ifdef IPV6
		if (rbracket) {
			int addrlen = rbracket - lbracket - 1;

			/* Check for valid length.
			 * addrlen >= sizeof(hostbuf) is theorically impossible
			 * but i keep the test in case of... Safer, imho --Zas */
			if (addrlen < 0 || addrlen > NI_MAXHOST) {
				internal("parse_url(): addrlen value is bad "
					"(%d) for URL '%s'. Problems are "
					"likely to be encountered. Please "
					"report this, it is a security bug!",
					addrlen, url);
				return -1;
			}
			if (host) *host = lbracket + 1;
			if (holen) *holen = addrlen;
		} else
#endif
		{
			if (host) *host = prefix_end;
			if (holen) *holen = host_end - prefix_end;
		}
	}

	if (*host_end == ':') { /* we have port here */
		unsigned char *port_end = host_end + 1 + strcspn(host_end + 1, "/");
		int idx;

		host_end++;

		if (port) *port = host_end;
		if (polen) *polen = port_end - host_end;

		/* test if port is number */
		/* TODO: possibly lookup for the service otherwise? --pasky */
		for (idx = 0; idx < port_end - host_end; idx++)
			if (host_end[idx] < '0' || host_end[idx] > '9')
				return -1;

		host_end = port_end;
	}

	if (data || dalen || post) {
		if (*host_end) host_end++; /* skip slash */

		prefix_end = strchr(host_end, POST_CHAR);
		if (data) *data = host_end;
		if (dalen) *dalen = prefix_end ? (prefix_end - host_end) : strlen(host_end);
		if (post) *post = prefix_end ? (prefix_end + 1) : NULL;
	}

	return 0;
}

/* Returns protocol part of url in an allocated string.
 * If url can't be parsed, it will return NULL. */
unsigned char *
get_protocol_name(unsigned char *url)
{
	int l;

	if (parse_url(url, &l,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL)) return NULL;

	return memacpy(url, l);
}


unsigned char *
get_host_and_pass(unsigned char *url, int include_port)
{
	unsigned char *user, *host, *port, *start, *end;
	int hostlen, portlen;

	if (parse_url(url, NULL,
		      &user, NULL,
		      NULL, NULL,
		      &host, &hostlen,
		      &port, &portlen,
		      NULL, NULL,
		      NULL)) return NULL;

	start = user ? user : host;
	end = (port && include_port) ? port + portlen : host + hostlen;
	return memacpy(start, end - start);
}


unsigned char *
get_host_name(unsigned char *url)
{
	unsigned char *h;
	int hl;

	if (parse_url(url, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      &h, &hl,
		      NULL, NULL,
		      NULL, NULL,
		      NULL)) return NULL;

	return memacpy(h, hl);
}


unsigned char *
get_user_name(unsigned char *url)
{
	unsigned char *h;
	int hl;

	if (parse_url(url, NULL,
		      &h, &hl,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL)) return NULL;

	return memacpy(h, hl);
}


unsigned char *
get_pass(unsigned char *url)
{
	unsigned char *h;
	int hl;

	if (parse_url(url, NULL,
		      NULL, NULL,
		      &h, &hl,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL)) return NULL;

	return memacpy(h, hl);
}


unsigned char *
get_port_str(unsigned char *url)
{
	unsigned char *p;
	int pl;

	if (parse_url(url, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      &p, &pl,
		      NULL, NULL,
		      NULL)) return NULL;

	return memacpy(p, pl);
}


/* XXX: Warning! We modify the p string here (altough it is reconstructed to
 * its original state at all exit points of the functions), which is somehow
 * controversial and it makes it impossible to pass constant strings to this
 * function. If this will be *that* painful, I'll make it allocate a working
 * tmp. copy, but for now I rather changed check_protocol() usage, which
 * obviously may not be the best thing :/. --pasky */
int
get_port(unsigned char *url)
{
	unsigned char *h;
	int hl;
	int n = -1;

	if (parse_url(url, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      &h, &hl,
		      NULL, NULL,
		      NULL)) return -1;

	if (h) {
		errno = 0;
		n = strtol(h, NULL, 10);
		if (!errno && n > 0) return n;
	}

	h = get_protocol_name(url);
	if (h) {
		if (*h) get_prot_info(h, &n, NULL, NULL);
		mem_free(h);
	}

	return n;
}


unsigned char *
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


/* This reconstructs URL with password stripped. */
unsigned char *
strip_url_password(unsigned char *url)
{
	unsigned char *str = init_str();
	int l = 0;

	int prlen;
	unsigned char *user;
	int uslen;
	unsigned char *pass;
	int palen;
	unsigned char *host;
	int holen;
	unsigned char *port;
	int polen;
	unsigned char *data;
	int dalen;
	int scheme;

	if (!str) return NULL;

	if (parse_url(url, &prlen, &user, &uslen, &pass, &palen, &host, &holen,
			   &port, &polen, &data, &dalen, NULL)
	    || !prlen) {
		/* Unknown protocol or mangled URL; keep the URL untouched. */
		mem_free(str);
		return stracpy(url);
	}

	scheme = check_protocol(url, prlen);

	if (scheme == SCHEME_UNKNOWN || get_protocol_free_syntax(scheme)) {
		/* Custom or unknown or free-syntax protocol;
		 * keep the URL untouched. */
		mem_free(str);
		return stracpy(url);
	}

	add_bytes_to_str(&str, &l, url, prlen);
	add_chr_to_str(&str, &l, ':');

	if (get_protocol_need_slashes(scheme))
		add_to_str(&str, &l, "//");

	if (user) {
		add_bytes_to_str(&str, &l, user, uslen);
		add_chr_to_str(&str, &l, '@');
	}

	if (host) {
#ifdef IPV6
		int brackets = !!memchr(host, ':', holen);

		if (brackets) add_chr_to_str(&str, &l, '[');
#endif
		add_bytes_to_str(&str, &l, host, holen);
#ifdef IPV6
		if (brackets) add_chr_to_str(&str, &l, ']');
#endif
	}

	if (port) {
		add_chr_to_str(&str, &l, ':');
		add_bytes_to_str(&str, &l, port, polen);
	}

	if (get_protocol_need_slash_after_host(scheme))
		add_chr_to_str(&str, &l, '/');

	if (dalen) {
		add_bytes_to_str(&str, &l, data, dalen);
	}

	return str;
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
	int l;
	int lo = !strncasecmp(base, "file://", 7); /* dsep() *hint* *hint* */
	int add_slash = 0;

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

	if (!parse_url(rel, &l,
		       NULL, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL)) {
		n = stracpy(rel);
		if (n) translate_directories(n);

		return n;
	}

	n = stracpy(rel);
	if (n) {
		while (n[0] && n[strlen(n) - 1] <= ' ') n[strlen(n) - 1] = 0;
		add_to_strn(&n, "/");

		if (!parse_url(n, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL)) {
			translate_directories(n);

			return n;
		}

		mem_free(n);
	}

prx:
	if (parse_url(base, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      NULL, NULL,
		      &path, NULL,
		      NULL) || !path) {
		internal("bad base url");
		return NULL;
	}

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

	/* Strip starting spaces */
	while (*url == ' ') url++;

	/* XXX: Why?! */
	if (!strncasecmp("proxy://", url, 8)) goto proxy;

	/* Ordinary parse */
	if (!parse_url(url, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL)) {
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
		if (!parse_url(newurl, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL)) {
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

		if (!parse_url(newurl, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL)) {
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
		if (!parse_url(newurl, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL, NULL,
			       NULL)) {
			insert_wd(&newurl, cwd);
			translate_directories(newurl);

			return newurl;
		}
	}

	/* ..and with slash */
	add_to_strn(&newurl, "/");
	if (!parse_url(newurl, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL, NULL,
		       NULL)) {
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
extract_proxy(unsigned char *url)
{
	unsigned char *a;

	if (strlen(url) < 8 || strncasecmp(url, "proxy://", 8))
		return url;

	a = strchr(url + 8, '/');
	if (!a) return url;

	return a + 1;
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

 	for (; *url && !end_of_dir(*url); url++) {
		if (*url == '.' && !extension)
			extension = url + 1;
		else if (dsep(*url))
			extension = NULL;
	}

	if (extension && extension < url)
		return memacpy(extension, url - extension);

	return NULL;
}

#undef dsep

/* Returns path+filename part (as is) from url as a dynamically allocated
 * string in name and length in namelen. */
void
get_filenamepart_from_url(unsigned char *url, unsigned char **name,
			  int *namelen)
{
	unsigned char *start, *end, *filename;
	int len;

	*name = NULL;
	*namelen = 0;

	for (start = url;
	     *start && *start != POST_CHAR && *start != ':';
	     start++);

	if (*start != ':' || *++start != '/' || *++start != '/') return;

	start++;

	for (end = start; *end && *end != POST_CHAR; end++);

	len = end - start;
	filename = mem_alloc(len + 1);

	if (!filename) return;

	if (len) memcpy(filename, start, len);
	filename[len] = '\0';

	*name = filename;
	*namelen = len;
}



/* URL encoding, escaping unallowed characters. */
static inline int
safe_char(unsigned char c)
{
	/* RFC 2396, Page 8, Section 2.3 ;-) */
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
	       || (c >= '0' && c <= '9')
	       || c == '-' || c == '_' || c == '.' || c == '!' || c == '~'
	       || c == '*' || c == '\''|| c == '(' || c == ')';
}


void
encode_url_string(unsigned char *name, unsigned char **data, int *len)
{
	unsigned char n[4];

	n[0] = '%';
	n[3] = '\0';

	for (; *name; name++) {
#if 0
		/* This is probably correct only for query part of URL..? */
		if (*name == ' ') add_chr_to_str(data, len, '+');
		else
#endif
		if (safe_char(*name)) {
			add_chr_to_str(data, len, *name);
		} else {
			/* Hex it. */
			n[1] = hx((((int) *name) & 0xF0) >> 4);
			n[2] = hx(((int) *name) & 0xF);
			add_to_str(data, len, n);
		}
	}
}


/* This function is evil, it modifies its parameter. */
/* XXX: but decoded string is _never_ longer than encoded string so it's an
 * efficient way to do that, imho. --Zas */
void
decode_url_string(unsigned char *src) {
	unsigned char *dst = src;
	unsigned char c;

	do {
		c = *src++;

		if (c == '%') {
			int x1 = unhx(*src);

			if (x1 >= 0) {
				int x2 = unhx(*(src + 1));

				if (x2 >= 0) {
					x1 = (x1 << 4) + x2;
					if (x1 != 0) { /* don't allow %00 */
						c = (unsigned char) x1;
						src += 2;
					}
				}
			}

		} else if (c == '+') {
			c = ' ';
		}

		*dst++ = c;
	} while (c != '\0');
}
