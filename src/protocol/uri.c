/* URL parser and translator; implementation of RFC 2396. */
/* $Id: uri.c,v 1.31 2003/07/23 00:47:11 pasky Exp $ */

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

#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


int
parse_uri(struct uri *uri, unsigned char *uristring)
{
	unsigned char *prefix_end, *host_end;
#ifdef IPV6
	unsigned char *lbracket, *rbracket;
#endif
	int protocol;

	assertm(uristring, "No uri to parse.");
	memset(uri, 0, sizeof(struct uri));

	/* Nothing to do for an empty url. */
	if_assert_failed return 0;
	if (!*uristring) return 0;
	uri->protocol = uristring;

	/* Isolate prefix */

	prefix_end = strchr(uristring, ':');
	if (!prefix_end) return 0;

	uri->protocollen = prefix_end - uristring;

	/* Get protocol */

	protocol = check_protocol(uristring, prefix_end - uristring);
	if (protocol == PROTOCOL_UNKNOWN) return 0;

	prefix_end++; /* ':' */

	/* Skip slashes */

	if (prefix_end[0] == '/' && prefix_end[1] == '/')
		prefix_end += 2;
	else if (get_protocol_need_slashes(protocol))
		return 0;

	if (get_protocol_free_syntax(protocol)) {
		uri->data = prefix_end;
		uri->datalen = strlen(prefix_end);
		return strlen(uri->protocol);
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
		unsigned char *user_end = strchr(prefix_end, ':');

		if (!user_end || user_end > host_end) {
			uri->user = prefix_end;
			uri->userlen = host_end - prefix_end;
		} else {
			uri->user = prefix_end;
			uri->userlen = user_end - prefix_end;
			uri->password = user_end + 1;
			uri->passwordlen = host_end - user_end - 1;
		}
		prefix_end = host_end + 1;
	}

#ifdef IPV6
	if (rbracket)
		host_end = rbracket + strcspn(rbracket, ":/?");
	else
#endif
		host_end = prefix_end + strcspn(prefix_end, ":/?");

	if (!*host_end && get_protocol_need_slash_after_host(protocol))
		return 0;

#ifdef IPV6
	if (rbracket) {
		int addrlen = rbracket - lbracket - 1;

		/* Check for valid length.
		 * addrlen >= sizeof(hostbuf) is theorically impossible
		 * but i keep the test in case of... Safer, imho --Zas */
		if (addrlen < 0 || addrlen > NI_MAXHOST) {
			internal("parse_uri(): addrlen value is bad "
				"(%d) for URL '%s'. Problems are "
				"likely to be encountered. Please "
				"report this, it is a security bug!",
				addrlen, uristring);
			return 0;
		}
		uri->host = lbracket + 1;
		uri->hostlen = addrlen;
	} else
#endif
	{
		uri->host = prefix_end;
		uri->hostlen = host_end - prefix_end;
	}

	if (*host_end == ':') { /* we have port here */
		unsigned char *port_end = host_end + 1 + strcspn(host_end + 1, "/");

		host_end++;

		uri->port = host_end;
		uri->portlen = port_end - host_end;

		/* test if port is number */
		/* TODO: possibly lookup for the service otherwise? --pasky */
		for (; host_end < port_end; host_end++)
			if (*host_end < '0' || *host_end > '9')
				return 0;
	}

	if (*host_end == '/') host_end++;

	prefix_end = strchr(host_end, POST_CHAR);
	uri->data = host_end;
	uri->datalen = prefix_end ? (prefix_end - host_end) : strlen(host_end);
	uri->post = prefix_end ? (prefix_end + 1) : NULL;

	return strlen(uri->protocol);
}

int
get_uri_port(struct uri *uri)
{
	int port = -1;

	if (uri->port && uri->portlen) {
		int n;

		errno = 0;
		n = strtol(uri->port, NULL, 10);
		if (!errno && n > 0) port = n;
	}

	if (port == -1) {
		enum protocol protocol;

		protocol = check_protocol(uri->protocol, uri->protocollen);
		if (protocol != PROTOCOL_UNKNOWN)
			port = get_protocol_port(protocol);
	}

	assertm(port != -1, "Invalid uri");
	/* Recovery path: we return -1 ;-). */
	return port;
}

/* We might need something more intelligent than this Swiss army knife. */
struct string *
add_uri_to_string(struct string *string, struct uri *uri,
		  enum uri_component components)
{
	enum protocol protocol = check_protocol(uri->protocol,
 						uri->protocollen);

 	assert(uri->protocol && uri->protocollen);
	if_assert_failed { return NULL; }

 	if (protocol == PROTOCOL_UNKNOWN
 	    || get_protocol_free_syntax(protocol)) {
 		/* Custom or unknown or free-syntax protocol;
 		 * keep the URI untouched. */
		add_to_string(string, uri->protocol);

		return string;
 	}

#define wants(x) (components & (x))

 	if (wants(URI_PROTOCOL)) {
		add_bytes_to_string(string, uri->protocol, uri->protocollen);
		add_char_to_string(string, ':');
 		if (get_protocol_need_slashes(protocol))
			add_to_string(string, "//");
 	}

 	if (wants(URI_USER) && uri->userlen) {
		add_bytes_to_string(string, uri->user, uri->userlen);

 		if (wants(URI_PASSWORD) && uri->passwordlen) {
			add_char_to_string(string, ':');
			add_bytes_to_string(string, uri->password,
						    uri->passwordlen);
 		}

		add_char_to_string(string, '@');
 	}

 	if (wants(URI_HOST) && uri->hostlen) {
#ifdef IPV6
 		int brackets = !!memchr(uri->host, ':', uri->hostlen);

		if (brackets) add_char_to_string(string, '[');
#endif
		add_bytes_to_string(string, uri->host, uri->hostlen);
#ifdef IPV6
		if (brackets) add_char_to_string(string, ']');
#endif
 	}

 	if (wants(URI_PORT)) {
 		if (uri->portlen) {
			add_char_to_string(string, ':');
			add_bytes_to_string(string, uri->port, uri->portlen);
 		}
#if 0
		/* We needs to add possibility to only add port if it's
		 * different from the default protocol port. */
		else if (protocol != PROTOCOL_USER) {
			/* For user protocols we don't know a default port.
			 * Should user protocols ports be configurable? */
			add_char_to_string(&string, ':');
			add_num_to_str(&string, get_protocol_port(protocol));
		}
#endif
	}

	if (get_protocol_need_slash_after_host(protocol))
		add_char_to_string(string, '/');

	if (wants(URI_DATA) && uri->datalen)
		add_bytes_to_string(string, uri->data, uri->datalen);

	if (wants(URI_POST) && uri->post)
		add_bytes_to_string(string, uri->post, strlen(uri->post));

#undef wants

	return string;
}

unsigned char *
get_uri_string(struct uri *uri, enum uri_component components)
{
	struct string string;

	if (!init_string(&string) && !add_uri_to_string(&string, uri, components))
		return NULL;

	return string.source;
}


struct string *
add_string_uri_to_string(struct string *string, unsigned char *uristring,
			 enum uri_component components)
{
	struct uri uri;

	if (!parse_uri(&uri, uristring))
		return NULL;

	return add_uri_to_string(string, &uri, components);
}

#define dsep(x) (lo ? dir_sep(x) : (x) == '/')

static void
translate_directories(unsigned char *uristring)
{
	unsigned char *src, *dest, *path;
	int lo = !strncasecmp(uristring, "file://", 7); /* dsep() *hint* *hint* */
	struct uri uri;

	if (!parse_uri(&uri, uristring) || !uri.data/* || *--url_data != '/'*/)
		return;

	path = uri.data;
	if (!dsep(*path)) path--;
	src = path;
	dest = path;

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

			if (src == path && (!src[2] || !src[3])) {
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

			while (dest > path) {
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
add_string_uri_filename_to_string(struct *string, unsigned char *uristring)
{
	unsigned char *s;
	int l;
	/* dsep() *hint* *hint* */
	int lo = !strncasecmp(uristring, "file://", 7);
	struct uri uri;

	if (!parse_uri(&uri, uristring))
		return NULL;

	if (uri.data) uristring = uri.data;
	s = uristring;
	while (*uristring && !end_of_dir(*uristring)) {
		if (dsep(*uristring)) s = uristring + 1;
		uristring++;
	}
	l = uristring - s;

	add_bytes_to_string(string, s, l);

	return string;
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

/* URI encoding, escaping unallowed characters. */
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
encode_uri_string(struct string *string, unsigned char *name)
{
	unsigned char n[4];

	n[0] = '%';
	n[3] = '\0';

	for (; *name; name++) {
#if 0
		/* This is probably correct only for query part of URI..? */
		if (*name == ' ') add_char_to_string(data, len, '+');
		else
#endif
		if (safe_char(*name)) {
			add_char_to_string(string, *name);
		} else {
			/* Hex it. */
			n[1] = hx((((int) *name) & 0xF0) >> 4);
			n[2] = hx(((int) *name) & 0xF);
			add_to_string(string, n);
		}
	}
}

#if 0
/* This function is evil, it modifies its parameter. */
/* XXX: but decoded string is _never_ longer than encoded string so it's an
 * efficient way to do that, imho. --Zas */
void
decode_uri_string(unsigned char *src) {
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
#endif
