/* URL parser and translator; implementation of RFC 2396. */
/* $Id: uri.c,v 1.105 2004/03/31 16:47:18 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
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
#include "util/file.h"
#include "util/hash.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"

int
end_with_known_tld(unsigned char *s, int slen)
{
	int i;
	static const unsigned char *tld[] =
	{ "com", "edu", "net",
	  "org", "gov", "mil",
	  "int", "biz", "arpa",
	  "aero", "coop",
	  "info", "museum",
	  "name", "pro", NULL };

	if (!slen) return -1;
	if (slen < 0) slen = strlen(s);

	for (i = 0; tld[i]; i++) {
		int tldlen = strlen(tld[i]);
		int pos = slen - tldlen;

		if (pos >= 0 && !strncasecmp(&s[pos], tld[i], tldlen))
			return pos;
	}

	return -1;
}

unsigned char *
get_protocol_end(const unsigned char *url)
{
	register unsigned char *end = (unsigned char *) url;

	/* Seek the end of the protocol name if any. */
	while (*end && *end != ':') {
		/* RFC1738:
		 * scheme  = 1*[ lowalpha | digit | "+" | "-" | "." ] */
		if ((*end >= 'a' && *end <= 'z') || (*end >= '0' && *end <= '9')
		    || *end == '+' || *end == '-' || *end == '.') {
			end++;
		} else
			break;

	}

	if (*end != ':' || end == url) return NULL; /* No valid protocol scheme. */

	return end;
}

int
parse_uri(struct uri *uri, unsigned char *uristring)
{
	unsigned char *prefix_end, *host_end;
#ifdef IPV6
	unsigned char *lbracket, *rbracket;
#endif
	enum protocol protocol;
	int known;

	assertm(uristring, "No uri to parse.");
	memset(uri, 0, sizeof(struct uri));

	/* Nothing to do for an empty url. */
	if_assert_failed return 0;
	if (!*uristring) return 0;
	uri->protocol_str = uristring;

	/* Check if protocol is known, and retrieve prefix_end. */
	protocol = known_protocol(uristring, &prefix_end);
	if (protocol == PROTOCOL_INVALID) return 0;

	known = (protocol != PROTOCOL_UNKNOWN);
	uri->protocollen = prefix_end - uristring;

	/* Set protocol */
	uri->protocol = protocol;

	prefix_end++; /* ':' */

	/* Skip slashes */

	if (prefix_end[0] == '/' && prefix_end[1] == '/')
		prefix_end += 2;
	else if (known && get_protocol_need_slashes(uri->protocol))
		return 0;

	if (!known || get_protocol_free_syntax(uri->protocol)) {
		uri->data = prefix_end;
		uri->datalen = strlen(prefix_end);
		return 1;
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

	if (known && !*host_end
	    && get_protocol_need_slash_after_host(uri->protocol))
		return 0;

#ifdef IPV6
	if (rbracket) {
		int addrlen = rbracket - lbracket - 1;

		/* Check for valid length.
		 * addrlen >= sizeof(hostbuf) is theorically impossible
		 * but i keep the test in case of... Safer, imho --Zas */
		assertm(addrlen >= 0 && addrlen < NI_MAXHOST,
			"parse_uri(): addrlen value is bad (%d) for URL '%s'. "
			"Problems are likely to be encountered. Please report "
			"this, it is a security bug!", addrlen, uristring);
		if_assert_failed return 0;

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

	return 1;
}

unsigned char *
unparse_uri(struct uri *uri)
{
	unsigned char *uristr = struri(*uri);

	memset(uri, 0, sizeof(struct uri));

	return uristr;
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

	if (port == -1 && uri->protocol != PROTOCOL_UNKNOWN) {
		port = get_protocol_port(uri->protocol);
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
	int known = (uri->protocol != PROTOCOL_UNKNOWN);

	assert(uri->protocol_str && uri->protocollen);
	if_assert_failed { return NULL; }

 	if (!known || get_protocol_free_syntax(uri->protocol)) {
 		/* Custom or unknown or free-syntax protocol;
 		 * keep the URI untouched. */
		add_to_string(string, struri(*uri));

		return string;
 	}

#define wants(x) (components & (x))

 	if (wants(URI_PROTOCOL)) {
		add_bytes_to_string(string, uri->protocol_str, uri->protocollen);
		add_char_to_string(string, ':');
 		if (get_protocol_need_slashes(uri->protocol))
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

	if (get_protocol_need_slash_after_host(uri->protocol))
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

	if (!init_string(&string)) return NULL;

	if (!add_uri_to_string(&string, uri, components))
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
	int lo;
	struct uri uri;

	if (!parse_uri(&uri, uristring) || !uri.data/* || *--url_data != '/'*/)
		return;

	/* dsep() *hint* *hint* */
	lo = (uri.protocol == PROTOCOL_FILE);

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

/* The standard URI comes in, and if the URI is not of the 'file' scheme, the
 * same URI comes out. However, for the file scheme, bastardized URI comes out
 * which consists of just the complete path to file/directory, which the dumb
 * 'file' protocol backend can understand. No host parts etc, that is what this
 * function is supposed to chew. */
static void
transform_file_url(unsigned char **up, unsigned char *cwd)
{
	unsigned char *url = *up;
	unsigned char *path;

	if (!url || !cwd || !*cwd
	    || strncasecmp(url, "file://", 7))
		return;
	url += 7; /* file:// */

	/* Sort out the host part. We currently support only host "localhost"
	 * (plus empty host part will be assumed to be "localhost" as well).
	 * As our extensions, '.' will reference to the cwd on localhost
	 * (originally, when the first thing after file:// wasn't "localhost/",
	 * we assumed the cwd as well, and pretended that there's no host part
	 * at all) and '..' to the directory parent to cwd. Another extension
	 * is that if this is a DOS-like system, the first char in two-char
	 * host part is uppercase letter and the second char is a colon, it is
	 * assumed to be a local disk specification. */
	/* TODO: Use FTP for non-localhost hosts. --pasky */

	/* For URL "file://", we open the current directory. Some other
	 * browsers instead open root directory, but AFAIK the standard does
	 * not specify that and this was the original behaviour and it is more
	 * consistent with our file://./ notation. */

	/* Who would name their file/dir '...' ? */
	if (url[0] == '.' || !url[0]) {
		int cwdlen = strlen(cwd);

		/* Insert the current working directory. */

		/* XXX: Post data copy. --zas */
		url = mem_alloc(strlen(*up) + cwdlen + 2);
		if (!url) return;

		memcpy(url, *up, 7);
		strcpy(url + 7, cwd);

		if (!dir_sep(cwd[cwdlen - 1])) strcat(url, "/");

		strcat(url, *up + 7);
		mem_free(*up);
		*up = url;

		return;
	}

#ifdef DOS_FS
	if (upcase(url[0]) >= 'A' && upcase(url[0]) <= 'Z'
	    && url[1] == ':' && dir_sep(url[2]))
		return;
#endif

	for (path = url; *path && !dir_sep(*path); path++);

	/* FIXME: We will in fact assume localhost even for non-local hosts,
	 * until we will support the FTP transformation. --pasky */

	memmove(url, path, strlen(path) + 1);
}

unsigned char *
join_urls(unsigned char *base, unsigned char *rel)
{
	unsigned char *p, *n, *path;
	int lo = !strncasecmp(base, "file://", 7); /* dsep() *hint* *hint* */
	int add_slash = 0;
	struct uri uri;
	int tmp;

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
			INTERNAL("bad base url: %s", base);
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
		int len = strlen(n);

		while (n[0] && n[len - 1] <= ' ') n[--len] = 0;
		add_to_strn(&n, "/");

		if (parse_uri(&uri, n)) {
			translate_directories(n);
			return n;
		}

		mem_free(n);
	}

prx:
	if (!parse_uri(&uri, base) || !uri.data) {
		INTERNAL("bad base url");
		return NULL;
	}
	path = uri.data;

	/* Either is path blank, but we've slash char before, or path is not
	 * blank, but doesn't start by a slash (if we'd just stay along with
	 * dsep(path[-1]) w/o all the surrounding crap, it should be enough,
	 * but I'm not sure and I don't want to break anything --pasky). */
	/* We skip first char of URL ('/') in parse_url() (ARGH). This
	 * is reason of all this bug-bearing magic.. */
	if (*path) {
		if (!dsep(*path)) path--;
	} else {
		if (dsep(path[-1])) path--;
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

	tmp = path - base;
	n = mem_alloc(tmp + strlen(rel) + add_slash + 1);
	if (!n) return NULL;

	memcpy(n, base, tmp);
	if (add_slash) n[tmp] = '/';
	strcpy(n + tmp + add_slash, rel);

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
		newurl = stracpy(url); /* XXX: Post data copy. */
		if (newurl) {
			transform_file_url(&newurl, cwd);
			translate_directories(newurl);
		}

		return newurl;
	}

	/* Try to add slash to end */
	if (strstr(url, "//") && (newurl = stracpy(url))) { /* XXX: Post data copy. */
		add_to_strn(&newurl, "/");
		if (parse_uri(&uri, newurl)) {
			transform_file_url(&newurl, cwd);
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
		unsigned char *expanded = expand_tilde(url);
		int not_file = 0;

		if (!expanded) return NULL;
		if (file_exists(expanded)) goto end;
#if 0
		/* This (not_file thing) is a bad assumption since @prefix is
		 * not changed which again causes any URI that is not
		 * recognized as being some other protocol to fallback to
		 * file:///. Changing the @prefix is also not good since it
		 * gives the error message "Bad URI syntax".  Leaving it like
		 * this will give a "No such file or directory" error. */
		else {
			not_file = 1;
			mem_free(expanded);
		}
#endif

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
			unsigned char *ipscan;

			/* Process the hostname */
			for (domain = ch + 1;
			     *(host_end = domain + strcspn(domain, ".:/")) == '.';
			     domain = host_end + 1);

			/* It's IP? */
			for (ipscan = ch; isdigit(*ipscan) || *ipscan == '.';
			     ipscan++);
			if (!*ipscan || *ipscan == ':' || *ipscan == '/')
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
				if (end_with_known_tld(domain, host_end - domain) >= 0)
					goto http;
			}
		}
end:
		newurl = stracpy(prefix);
		if (!newurl) return NULL;
		if (!not_file) {
			if (!dir_sep(*expanded)) add_to_strn(&newurl, "./");
			add_to_strn(&newurl, expanded);
			mem_free(expanded);
		} else {
			add_to_strn(&newurl, url); /* XXX: Post data copy. */
		}

		if (not_file && !strchr(url, '/')) add_to_strn(&newurl, "/");

		if (parse_uri(&uri, newurl)) {
			transform_file_url(&newurl, cwd);
			translate_directories(newurl);

			return newurl;
		}

		mem_free(newurl);
		return NULL;
	}

	newurl = memacpy(url, ch - url + 1); /* XXX: Post data copy. */
	if (!newurl) return NULL;

	/* Try prefix:some.url -> prefix://some.url.. */
	if (strncmp(ch + 1, "//", 2)) {
		add_to_strn(&newurl, "//");
		add_to_strn(&newurl, ch + 1);
		if (!parse_uri(&uri, newurl)) {
			transform_file_url(&newurl, cwd);
			translate_directories(newurl);

			return newurl;
		}
	}

	/* ..and with slash */
	add_to_strn(&newurl, "/");
	if (parse_uri(&uri, newurl)) {
		transform_file_url(&newurl, cwd);
		translate_directories(newurl);

		return newurl;
	}

	mem_free(newurl);
	return NULL;
}

unsigned char *
extract_fragment(unsigned char *uri)
{
	unsigned char *fragment, *frag_start, *post_start;
	size_t frag_len;

	assert(uri);
	if_assert_failed return NULL;

	/* Empty string ? */
	if (!*uri) return NULL;

	/* Is there a fragment part in uri ? */
	frag_start = strchr(uri, '#');
	if (!frag_start) return NULL;

	/* Copy fragment string (without '#') and without trailing
	 * post data if any. */
	fragment = get_no_post_url(frag_start + 1, &frag_len);

	/* Start position of post data if any. */
	post_start = frag_start + frag_len + 1;

	/* Even though fragment wasn't allocated remove it from the @uri. */
	memmove(frag_start, post_start, strlen(post_start) + 1);

	return fragment;
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

struct string *
add_string_uri_filename_to_string(struct string *string, unsigned char *uristring)
{
	unsigned char *filename;
	unsigned char *pos;
	int lo;
	struct uri uri;

	if (!parse_uri(&uri, uristring))
		return NULL;

	assert(uri.data);
	/* dsep() *hint* *hint* */
	lo = (uri.protocol == PROTOCOL_FILE);

	for (pos = filename = uri.data; *pos && !end_of_dir(*pos); pos++)
		if (dsep(*pos))
			filename = pos + 1;

	return add_bytes_to_string(string, filename, pos - filename);
}

unsigned char *
get_extension_from_url(unsigned char *url)
{
	int lo = !strncasecmp(url, "file://", 7); /* dsep() *hint* *hint* */
	unsigned char *extension = NULL;
	int afterslash = 1;

 	for (; *url && !end_of_dir(*url); url++) {
		if (!afterslash && !extension && *url == '.') {
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
			add_bytes_to_string(string, n, sizeof(n) - 1);
		}
	}
}

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
			/* %7E */
			if (src[0] == '7' && src[1] == 'E') goto next;
			else {
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
			}

#if 0
		} else if (c == '+') {
			/* As the comment in encode_uri_string suggests, '+'
			 * should only be decoded in the query part of a URI
			 * (should that be 'URL'?). I'm not bold enough to
			 * disable this code, tho. -- Miciah */
			c = ' ';
#endif
		}
next:
		*dst++ = c;
	} while (c != '\0');
}

int
get_no_post_url_length(unsigned char *url)
{
	unsigned char *postchar = strchr(url, POST_CHAR);
	int len = postchar ? postchar - url : strlen(url);

	return len;
}

unsigned char *
post_data_start(unsigned char *url)
{
	return strchr(url, POST_CHAR);
}

unsigned char *
get_no_post_url(unsigned char *url, int *url_len)
{
	int len = get_no_post_url_length(url);

	if (url_len) *url_len = len;

	return memacpy(url, len);
}


/* URI cache */

struct uri_cache_entry {
	struct uri uri;
	unsigned char string[1];
};

struct uri_cache {
	struct hash *map;
	unsigned int refcount;
};

static struct uri_cache uri_cache;

static inline struct uri_cache_entry *
get_uri_cache_entry(unsigned char *string, int length)
{
	struct hash_item *item = get_hash_item(uri_cache.map, string, length);
	struct uri_cache_entry *entry;

	if (item) return item->value;

	/* Setup a new entry */

	entry = mem_calloc(1, sizeof(struct uri_cache_entry) + length);
	if (!entry) return NULL;

	object_nolock(&entry->uri);
	memcpy(&entry->string, string, length);
	string = entry->string;

	if (!parse_uri(&entry->uri, string)
	    || !add_hash_item(uri_cache.map, string, length, entry)) {
		mem_free(entry);
		return NULL;
	}

	object_lock(&uri_cache);

	return entry;
}

struct uri *
get_uri(unsigned char *string)
{
	struct uri_cache_entry *entry;

	if (!is_object_used(&uri_cache)) {
		uri_cache.map = init_hash(hash_size(3), strhash);
		if (!uri_cache.map) return NULL;
	}

	entry = get_uri_cache_entry(string, strlen(string));
	if (!entry) {
		if (!is_object_used(&uri_cache))
			free_hash(uri_cache.map);
		return NULL;
	}

	object_lock(&entry->uri);

	return &entry->uri;
}

void
done_uri(struct uri *uri)
{
	unsigned char *string = struri(*uri);
	int length = strlen(string);
	struct hash_item *item;
	struct uri_cache_entry *entry;

	assert(is_object_used(&uri_cache));

	object_unlock(uri);
	if (is_object_used(uri)) return;

	item = get_hash_item(uri_cache.map, string, length);
	entry = item ? item->value : NULL;

	assertm(entry, "Releasing unknown URI [%s]", string);
	del_hash_item(uri_cache.map, item);
	mem_free(entry);

	/* Last URI frees the cache */
	object_unlock(&uri_cache);
	if (!is_object_used(&uri_cache))
		free_hash(uri_cache.map);
}
