/* URL parser and translator; implementation of RFC 2396. */
/* $Id: uri.c,v 1.217 2004/05/31 23:56:39 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#ifdef HAVE_IDNA_H
#include <idna.h>
#endif
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


static inline int
end_of_dir(struct uri *uri, unsigned char c)
{
	/* Is it correct to assume file:// URIs won't have query parts? What
	 * about CGI?
	 *
	 * Else the problem with handling '?' in file:// URIs is that
	 * get_extension_from_uri() won't get it right. So maybe a cheap hack
	 * for that function would do the trick instead.
	 *
	 * Reported by Grzegorz Adam Hankiewicz <gradha@titanium.sabren.com>
	 * Thu, May 27, 2004 on elinks-users mailing list. --jonas */
	return c == POST_CHAR || c == '#' || c == ';'
		|| (uri->protocol != PROTOCOL_FILE && c == '?');
}

static inline int
is_uri_dir_sep(struct uri *uri, unsigned char pos)
{
	return (uri->protocol == PROTOCOL_FILE ? dir_sep(pos) : pos == '/');
}


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

static inline int
get_protocol_length(const unsigned char *url)
{
	register unsigned char *end = (unsigned char *) url;

	/* Seek the end of the protocol name if any. */
	/* RFC1738:
	 * scheme  = 1*[ lowalpha | digit | "+" | "-" | "." ] */
	while (isalpha(*end) || isdigit(*end)
		|| *end == '+' || *end == '-' || *end == '.')
		end++;

	/* Also return 0 if there's no protocol name (@end == @url). */
	return (*end == ':') ? end - url : 0;
}

/* Tcp port range */
#define LOWEST_PORT 0
#define HIGHEST_PORT 65535

enum uri_errno
parse_uri(struct uri *uri, unsigned char *uristring)
{
	unsigned char *prefix_end, *host_end;
#ifdef CONFIG_IPV6
	unsigned char *lbracket, *rbracket;
#endif
	int known;

	assertm(uristring, "No uri to parse.");
	memset(uri, 0, sizeof(struct uri));

	/* Nothing to do for an empty url. */
	if_assert_failed return 0;
	if (!*uristring) return URI_ERRNO_EMPTY;

	uri->string = uristring;
	uri->protocollen = get_protocol_length(uristring);

	/* Invalid */
	if (!uri->protocollen) return URI_ERRNO_INVALID_PROTOCOL;

	/* Figure out whether the protocol is known */
	uri->protocol = get_protocol(struri(uri), uri->protocollen);
	known = (uri->protocol != PROTOCOL_UNKNOWN);

	prefix_end = uristring + uri->protocollen + 1; /* ':' */

	/* Skip slashes */

	if (prefix_end[0] == '/' && prefix_end[1] == '/')
		prefix_end += 2;
	else if (known && get_protocol_need_slashes(uri->protocol))
		return URI_ERRNO_NO_SLASHES;

	if (!known || get_protocol_free_syntax(uri->protocol)) {
		uri->data = prefix_end;
		uri->datalen = strlen(prefix_end);
		return URI_ERRNO_OK;
	}

	/* Isolate host */

#ifdef CONFIG_IPV6
	/* Get brackets enclosing IPv6 address */
	lbracket = strchr(prefix_end, '[');
	if (lbracket) {
		rbracket = strchr(lbracket, ']');
		/* [address] is handled only inside of hostname part (surprisingly). */
		if (rbracket && rbracket < prefix_end + strcspn(prefix_end, "/"))
			uri->ipv6 = 1;
		else
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

#ifdef CONFIG_IPV6
	if (uri->ipv6)
		host_end = rbracket + strcspn(rbracket, ":/?");
	else
#endif
		host_end = prefix_end + strcspn(prefix_end, ":/?");

#ifdef CONFIG_IPV6
	if (uri->ipv6) {
		int addrlen = rbracket - lbracket - 1;

		/* Check for valid length.
		 * addrlen >= sizeof(hostbuf) is theorically impossible
		 * but i keep the test in case of... Safer, imho --Zas */
		assertm(addrlen >= 0 && addrlen < NI_MAXHOST,
			"parse_uri(): addrlen value is bad (%d) for URL '%s'. "
			"Problems are likely to be encountered. Please report "
			"this, it is a security bug!", addrlen, uristring);
		if_assert_failed return URI_ERRNO_IPV6_SECURITY;

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

		/* We only use 8 bits for portlen so better check */
		if (uri->portlen != port_end - host_end)
			return URI_ERRNO_INVALID_PORT;

		/* test if port is number */
		/* TODO: possibly lookup for the service otherwise? --pasky */
		for (; host_end < port_end; host_end++)
			if (*host_end < '0' || *host_end > '9')
				return URI_ERRNO_INVALID_PORT;

		/* Check valid port value, and let show an error message
		 * about invalid url syntax. */
		if (uri->port && uri->portlen) {
			int n;

			errno = 0;
			n = strtol(uri->port, NULL, 10);
			if (errno || n < LOWEST_PORT || n > HIGHEST_PORT)
				return URI_ERRNO_INVALID_PORT;
		}
	}

	if (*host_end == '/') {
		host_end++;

	} else if (get_protocol_need_slash_after_host(uri->protocol)) {
		return URI_ERRNO_NO_HOST_SLASH;
	}

	prefix_end = strchr(host_end, POST_CHAR);
	uri->data = host_end;
	uri->datalen = prefix_end ? (prefix_end - host_end) : strlen(host_end);
	uri->post = prefix_end ? (prefix_end + 1) : NULL;

	return URI_ERRNO_OK;
}

int
get_uri_port(struct uri *uri)
{
	if (uri->port && uri->portlen) {
		unsigned char *end = uri->port;
		int port = strtol(uri->port, (char **) &end, 10);

		if (end != uri->port) {
			assert(port >= LOWEST_PORT && port <= HIGHEST_PORT);
			return port;
		}
	}

	return get_protocol_port(uri->protocol);
}

#define can_compare_uri_components(comp) \
	((comp) == ((comp) & (URI_PROTOCOL | URI_USER | URI_PASSWORD | URI_HOST | URI_PORT)))

static inline int
compare_component(unsigned char *a, int alen, unsigned char *b, int blen)
{
	/* Check that the length and the strings are both set or unset */
	if (alen != blen || !!a != !!b) return 0;

	/* Both are unset so that will make a perfect match */
	if (!a || !alen) return 1;

	/* Let the higher forces decide */
	return !memcmp(a, b, blen);
}

#define wants(x) (components & (x))

int
compare_uri(struct uri *a, struct uri *b, enum uri_component components)
{
	if (a == b) return 1;
	if (!components) return 0;

	assertm(can_compare_uri_components(components),
		"compare_uri() is a work in progress. Component unsupported");

	if (wants(URI_PROTOCOL)
	    && a->protocol != b->protocol)
		return 0;

	if (wants(URI_USER)
	    && !compare_component(a->user, a->userlen, b->user, b->userlen))
		return 0;

	if (wants(URI_PASSWORD)
	    && !compare_component(a->password, a->passwordlen, b->password, b->passwordlen))
		return 0;

	if (wants(URI_HOST)
	    && !compare_component(a->host, a->hostlen, b->host, b->hostlen))
		return 0;

	if (wants(URI_PORT)
	    && !compare_component(a->port, a->portlen, b->port, b->portlen))
		return 0;

	return 1;
}


/* We might need something more intelligent than this Swiss army knife. */
struct string *
add_uri_to_string(struct string *string, struct uri *uri,
		  enum uri_component components)
{
	/* Custom or unknown keep the URI untouched. */
	if (uri->protocol == PROTOCOL_UNKNOWN)
		return add_to_string(string, struri(uri));

 	if (wants(URI_PROTOCOL)) {
		add_bytes_to_string(string, uri->string, uri->protocollen);
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
#ifdef CONFIG_IPV6
		if (uri->ipv6) add_char_to_string(string, '[');
#endif
#ifdef CONFIG_IDN
		/* Support for the GNU International Domain Name library.
		 *
		 * http://www.gnu.org/software/libidn/manual/html_node/IDNA-Functions.html
		 *
		 * Now it is probably not perfect because idna_to_ascii_lz()
		 * will be using a ``zero terminated input string encoded in
		 * the current locale's character set''. Anyway I don't know
		 * how to convert anything to UTF-8 or Unicode. --jonas */
		if (wants(URI_IDN)) {
			unsigned char *host = memacpy(uri->host, uri->hostlen);

			if (host) {
				char *idname;
				int code = idna_to_ascii_lz(host, &idname, 0);

				/* FIXME: Return NULL if it coughed? --jonas */
				if (code == IDNA_SUCCESS) {
					add_to_string(string, idname);
					free(idname);
				}

				mem_free(host);
			}
		} else
#endif
			add_bytes_to_string(string, uri->host, uri->hostlen);
#ifdef CONFIG_IPV6
		if (uri->ipv6) add_char_to_string(string, ']');
#endif
 	}

 	if (wants(URI_PORT) || wants(URI_DEFAULT_PORT)) {
 		if (uri->portlen) {
			add_char_to_string(string, ':');
			add_bytes_to_string(string, uri->port, uri->portlen);

		} else if (wants(URI_DEFAULT_PORT)
			   && uri->protocol != PROTOCOL_USER) {
			/* For user protocols we don't know a default port.
			 * Should user protocols ports be configurable? */
			int port = get_protocol_port(uri->protocol);

			add_char_to_string(string, ':');
			add_long_to_string(string, port);
		}
	}

	/* Only add slash if we need to separate */
	if ((wants(URI_DATA) || wants(URI_POST))
	    && wants(~(URI_DATA | URI_PORT))
	    && get_protocol_need_slash_after_host(uri->protocol))
		add_char_to_string(string, '/');

	if (wants(URI_DATA) && uri->datalen)
		add_bytes_to_string(string, uri->data, uri->datalen);

	/* We can not test uri->datalen here since we need to always
	 * add '/'. */
	if (wants(URI_PATH) || wants(URI_FILENAME)) {
		unsigned char *filename = uri->data;
		unsigned char *pos;

		assertm(!wants(URI_PATH) || components == URI_PATH,
			"URI_PATH should be used alone %d", components);
		assertm(!wants(URI_FILENAME) || components == URI_FILENAME,
			"URI_FILENAME should be used alone %d", components);

		if (wants(URI_PATH) && !is_uri_dir_sep(uri, *filename)) {
			/* FIXME: Add correct separator */
			add_char_to_string(string, '/');
		}

		if (!uri->datalen) return string;

		for (pos = filename; *pos && !end_of_dir(uri, *pos); pos++)
			if (wants(URI_FILENAME) && is_uri_dir_sep(uri, *pos))
				filename = pos + 1;

		return add_bytes_to_string(string, filename, pos - filename);
	}

	if (wants(URI_QUERY) && uri->datalen) {
		unsigned char *query = memchr(uri->data, '?', uri->datalen);

		assertm(URI_QUERY == components,
			"URI_QUERY should be used alone %d", components);

		if (!query) return string;

		query++;
		/* Check fragment and POST_CHAR */
		return add_bytes_to_string(string, query, strcspn(query, "#\001"));
	}

	if (wants(URI_POST) && uri->post) {
		add_char_to_string(string, POST_CHAR);
		add_to_string(string, uri->post);
	}

	return string;
}

#undef wants

unsigned char *
get_uri_string(struct uri *uri, enum uri_component components)
{
	struct string string;

	if (init_string(&string)
	    && add_uri_to_string(&string, uri, components))
		return string.source;

	done_string(&string);
	return NULL;
}


struct string *
add_string_uri_to_string(struct string *string, unsigned char *uristring,
			 enum uri_component components)
{
	struct uri uri;

	if (parse_uri(&uri, uristring) != URI_ERRNO_OK)
		return NULL;

	return add_uri_to_string(string, &uri, components);
}


#define normalize_uri_reparse(uri, str)	normalize_uri(uri, str, 1)
#define normalize_uri_noparse(uri)	normalize_uri(uri, struri(uri), 0)

static unsigned char *
normalize_uri(struct uri *uri, unsigned char *uristring, int parse)
{
	unsigned char *parse_string = uristring;
	unsigned char *src, *dest, *path;
	int need_slash = 0;

	/* We need to get the real (proxied) URI but lowercase relevant URI
	 * parts along the way. */
	do {
		if (parse && parse_uri(uri, parse_string) != URI_ERRNO_OK)
			return uristring;

		assert(uri->data);

		/* This is a maybe not the right place but both join_urls() and
		 * get_translated_uri() through translate_url() calls this
		 * function and then it already works on and modifies an
		 * allocated copy. */
		convert_to_lowercase(uri->string, uri->protocollen);
		if (uri->hostlen) convert_to_lowercase(uri->host, uri->hostlen);

		parse = 1;
		parse_string = uri->data;
	} while (uri->protocol == PROTOCOL_PROXY);

	if (uri->protocol != PROTOCOL_UNKNOWN)
		need_slash = get_protocol_need_slash_after_host(uri->protocol);

	/* We want to start at the first slash to also reduce URIs like
	 * http://host//index.html to http://host/index.html */
	path = uri->data - need_slash;
	dest = src = path;

	/* This loop mangles the URI string by removing directory elevators and
	 * other cruft. Example: /.././etc////..//usr/ -> /usr/ */
	while (*dest) {
		/* If the following pieces are the LAST parts of URL, we remove
		 * them as well. See RFC 1808 for details. */

		if (end_of_dir(uri, src[0])) {
			/* URL data contains no more path. */
			memmove(dest, src, strlen(src) + 1);
			break;
		}

		if (!is_uri_dir_sep(uri, src[0])) {
			/* This is to reduce indentation */

		} else if (src[1] == '.') {
			if (!src[2]) {
				/* /. - skip the dot */
				*dest++ = *src;
				*dest = 0;
				break;

			} else if (is_uri_dir_sep(uri, src[2])) {
				/* /./ - strip that.. */
				src += 2;
				continue;

			} else if (src[2] == '.' && is_uri_dir_sep(uri, src[3])) {
				/* /../ - strip that and preceding element. */

				/* First back out the last incrementation of
				 * @dest (dest++) to get the position that was
				 * last asigned to. */
				if (dest > path) dest--;

				/* @dest might be pointing to a dir separator
				 * so we decrement before any testing. */
				while (dest > path) {
					dest--;
					if (is_uri_dir_sep(uri, *dest)) break;
				}

				src += 3;
				continue;

			} else if (src[2] == '.' && !src[3]) {
				/* /.. - nothing to skip */
				*dest++ = *src;
				*dest = 0;
				break;
			}

		} else if (is_uri_dir_sep(uri, src[1])) {
			/* // - ignore first '/'. */
			src += 1;
			continue;
		}

		/* We don't want to access memory past the NUL char. */
		*dest = *src++;
		if (*dest) dest++;
	}

	return uristring;
}

/* The 'file' scheme URI comes in and bastardized URI comes out which consists
 * of just the complete path to file/directory, which the dumb 'file' protocol
 * backend can understand. No host parts etc, that is what this function is
 * supposed to chew. */
static struct uri *
transform_file_url(struct uri *uri, unsigned char *cwd)
{
	unsigned char *path = uri->data;

	assert(uri->protocol == PROTOCOL_FILE && uri->data);

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
	if (*path == '.' || !*path) {
		int cwdlen = strlen(cwd);

		/* Either we will end up with '//' and translate_directories()
		 * will shorten it or the '/' will mark the inserted cwd as a
		 * directory. */
		if (*path == '.') *path = '/';

		/* Insert the current working directory. */
		insert_in_string(&struri(uri), 7, cwd, cwdlen);
		return uri;
	}

#ifdef DOS_FS
	if (upcase(path[0]) >= 'A' && upcase(path[0]) <= 'Z'
	    && path[1] == ':' && dir_sep(path[2]))
		return NULL;
#endif

	for (; *path && !dir_sep(*path); path++);

	/* FIXME: We will in fact assume localhost even for non-local hosts,
	 * until we will support the FTP transformation. --pasky */

	memmove(uri->data, path, strlen(path) + 1);
	return uri;
}

static unsigned char *translate_url(unsigned char *url, unsigned char *cwd);

unsigned char *
join_urls(unsigned char *base, unsigned char *rel)
{
	unsigned char *p, *n, *path;
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

		return normalize_uri_reparse(&uri, n);
	} else if (rel[0] == '?') {
		n = stracpy(base);
		if (!n) return NULL;

		for (p = n; *p && *p != POST_CHAR && *p != '?' && *p != '#'; p++);
		*p = '\0';
		add_to_strn(&n, rel);

		return normalize_uri_reparse(&uri, n);
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

	/* Check if there is some protocol name to go for */
	tmp = get_protocol_length(rel);
	if (tmp && get_protocol(rel, tmp) != PROTOCOL_UNKNOWN) {
		n = translate_url(rel, NULL);
		if (n) return n;
	}

prx:
	if (parse_uri(&uri, base) != URI_ERRNO_OK || !uri.data) {
		INTERNAL("bad base url");
		return NULL;
	}
	path = uri.data;

	/* Either is path blank, but we've slash char before, or path is not
	 * blank, but doesn't start by a slash (if we'd just stay along with
	 * is_uri_dir_sep(&uri, path[-1]) w/o all the surrounding crap, it
	 * should be enough, but I'm not sure and I don't want to break
	 * anything --pasky). */
	/* We skip first char of URL ('/') in parse_url() (ARGH). This
	 * is reason of all this bug-bearing magic.. */
	if (*path) {
		if (!is_uri_dir_sep(&uri, *path)) path--;
	} else {
		if (is_uri_dir_sep(&uri, path[-1])) path--;
	}

	if (!is_uri_dir_sep(&uri, rel[0])) {
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
			if (end_of_dir(&uri, *path_end)) break;
			/* Modify the path pointer, so that it'll always point
			 * above the last '/' in the URL; later, we'll copy the
			 * URL only _TO_ this point, and anything after last
			 * slash will be substituted by 'rel'. */
			if (is_uri_dir_sep(&uri, *path_end)) path = path_end + 1;
		}
	}

	tmp = path - base;
	n = mem_alloc(tmp + strlen(rel) + add_slash + 1);
	if (!n) return NULL;

	memcpy(n, base, tmp);
	if (add_slash) n[tmp] = '/';
	strcpy(n + tmp + add_slash, rel);

	return normalize_uri_reparse(&uri, n);
}


/* Tries to figure out what protocol @newurl might be specifying by checking if
 * it exists as a file locally or by checking parts of the host name. */
static inline enum protocol
find_uri_protocol(unsigned char *newurl)
{
	unsigned char *ch;

	/* First see if it is a file so filenames that look like hostnames
	 * won't confuse us below. */
	if (file_exists(newurl)) return PROTOCOL_FILE;

	/* Yes, it would be simpler to make test for IPv6 address first,
	 * but it would result in confusing mix of ifdefs ;-). */
	/* FIXME: Handle irc. and other common hostnames? --jonas */

	ch = newurl + strcspn(newurl, ".:/@");
	if (*ch == '@' || (*ch == ':' && *newurl != '[')
		|| !strncasecmp(newurl, "ftp.", 4)) {
		/* Contains user/password/ftp-hostname */
		return PROTOCOL_FTP;

#ifdef CONFIG_IPV6
	} else if (*newurl == '[' && *ch == ':') {
		/* Candidate for IPv6 address */
		unsigned char *bracket2, *colon2;

		ch++;
		bracket2 = strchr(ch, ']');
		colon2 = strchr(ch, ':');
		if (bracket2 && colon2 && bracket2 > colon2)
			return PROTOCOL_HTTP;
#endif

	} else if (*newurl != '.' && *ch == '.') {
		/* Contains domain name? */
		unsigned char *host_end, *domain;
		unsigned char *ipscan;

		/* Process the hostname */
		for (domain = ch + 1;
			*(host_end = domain + strcspn(domain, ".:/?")) == '.';
			domain = host_end + 1);

		/* It's IP? */
		for (ipscan = ch; isdigit(*ipscan) || *ipscan == '.';
			ipscan++);

		if (!*ipscan || *ipscan == ':' || *ipscan == '/')
			return PROTOCOL_HTTP;

		/* It's two-letter or known TLD? */
		if (host_end - domain == 2
		    || end_with_known_tld(domain, host_end - domain) >= 0)
			return PROTOCOL_HTTP;
	}

	/* We default to file:// even though we already tested if the file
	 * existed since it will give a "No such file or directory" error.
	 * which might better hint the user that there was problem figuring out
	 * the URI. */
	return PROTOCOL_FILE;
}

/* Returns an URI string that can be used internally. Adding protocol prefix,
 * missing slashes etc. */
static unsigned char *
translate_url(unsigned char *url, unsigned char *cwd)
{
	unsigned char *newurl;
	struct uri uri;
	enum uri_errno uri_errno, prev_errno = URI_ERRNO_EMPTY;

	/* Strip starting spaces */
	while (*url == ' ') url++;
	if (!*url) return NULL;

	newurl = expand_tilde(url); /* XXX: Post data copy. */
	if (!newurl) return NULL;

parse_uri:
	/* Yay a goto loop. If we get some URI parse error and try to
	 * fix it we go back to here and try again. */
	/* Ordinary parse */
	uri_errno = parse_uri(&uri, newurl);

	/* Bail out if the same error occurs twice */
	if (uri_errno == prev_errno) {
		mem_free(newurl);
		return NULL;
	}

	prev_errno = uri_errno;

	switch (uri_errno) {
	case URI_ERRNO_OK:
		/* If file:// URI is transformed we need to reparse. */
		if (uri.protocol == PROTOCOL_FILE && cwd && *cwd
		    && transform_file_url(&uri, cwd))
			return normalize_uri_reparse(&uri, struri(&uri));

		/* Translate the proxied URI too if proxy:// */
		if (uri.protocol == PROTOCOL_PROXY) {
			unsigned char *data = translate_url(uri.data, cwd);
			int pos = uri.data - struri(&uri);

			if (!data) break;
			struri(&uri)[pos] = 0;
			insert_in_string(&struri(&uri), pos, data, strlen(data));
			mem_free(data);
			return normalize_uri_reparse(&uri, struri(&uri));
		}

		return normalize_uri_noparse(&uri);

	case URI_ERRNO_NO_SLASHES:
		/* Try prefix:some.url -> prefix://some.url.. */
		insert_in_string(&newurl, uri.protocollen + 1, "//", 2);
		goto parse_uri;

	case URI_ERRNO_NO_HOST_SLASH:
	{
		int offset = get_uri_hostlen(&uri, struri(&uri));

		assertm(uri.host, "uri.host not set after no host slash error");
		insert_in_string(&newurl, offset, "/", 1);
		goto parse_uri;
	}

	case URI_ERRNO_INVALID_PROTOCOL:
	{
		/* No protocol name */
		enum protocol protocol = find_uri_protocol(newurl);
		unsigned char *prefix;

		switch (protocol) {
			case PROTOCOL_FTP:
				prefix = "ftp://";
				break;

			case PROTOCOL_HTTP:
				prefix = "http://";
				break;

			case PROTOCOL_FILE:
			default:
				prefix = "file://";
				if (!dir_sep(*newurl))
					insert_in_string(&newurl, 0, "./", 2);
		}

		insert_in_string(&newurl, 0, prefix, strlen(prefix));
		goto parse_uri;
	}
	case URI_ERRNO_EMPTY:
	case URI_ERRNO_IPV6_SECURITY:
	case URI_ERRNO_INVALID_PORT:
	case URI_ERRNO_INVALID_PORT_RANGE:
		/* None of these can be handled properly. */
		break;
	}

	mem_free(newurl);
	return NULL;
}


static inline unsigned char *
extract_fragment(unsigned char *uri)
{
	unsigned char *fragment, *frag_start, *post_start;
	int frag_len;

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

struct uri *
get_translated_uri(unsigned char *uristring, unsigned char *cwd,
		   unsigned char **fragment)
{
	struct uri *uri;

	uristring = cwd ? translate_url(uristring, cwd) : stracpy(uristring);
	if (!uristring) return NULL;

	if (fragment) *fragment = extract_fragment(uristring);
	uri = get_uri(uristring, -1);
	mem_free(uristring);
	if (!uri && fragment) mem_free_set(&*fragment, NULL);

	return uri;
}


unsigned char *
get_extension_from_uri(struct uri *uri)
{
	unsigned char *extension = NULL;
	int afterslash = 1;
	unsigned char *pos = uri->data;

	assert(pos);

	for (; *pos && !end_of_dir(uri, *pos); pos++) {
		if (!afterslash && !extension && *pos == '.') {
			extension = pos + 1;
		} else if (is_uri_dir_sep(uri, *pos)) {
			extension = NULL;
			afterslash = 1;
		} else {
			afterslash = 0;
		}
	}

	if (extension && extension < pos)
		return memacpy(extension, pos - extension);

	return NULL;
}

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
get_no_post_url(unsigned char *url, int *url_len)
{
	int len = get_no_post_url_length(url);

	if (url_len) *url_len = len;

	return memacpy(url, len);
}


/* URI list */

#define URI_LIST_GRANULARITY 0x3

#define realloc_uri_list(list) \
	mem_align_alloc(&(list)->uris, (list)->size, (list)->size + 1, \
			struct uri *, URI_LIST_GRANULARITY)

struct uri *
add_to_uri_list(struct uri_list *list, struct uri *uri)
{
	if (!realloc_uri_list(list))
		return NULL;

	list->uris[list->size++] = get_uri_reference(uri);

	return uri;
};

void
free_uri_list(struct uri_list *list)
{
	struct uri *uri;
	int index;

	if (!list->uris) return;

	foreach_uri (uri, index, list)
		done_uri(uri);

	mem_free(list->uris);
}

/* URI cache */

struct uri_cache_entry {
	struct uri uri;
	unsigned char string[1];
};

struct uri_cache {
	struct hash *map;
	struct object object;
};

static struct uri_cache uri_cache;

#ifdef CONFIG_DEBUG
static inline void
check_uri_sanity(struct uri *uri)
{
	int pos;

	for (pos = 0; pos < uri->protocollen; pos++)
		if (isupper(uri->string[pos])) goto error;

	if (uri->hostlen)
		for (pos = 0; pos < uri->hostlen; pos++)
			if (isupper(uri->host[pos])) goto error;
	return;
error:
	INTERNAL("Uppercase letters detected in protocol or host part.");
}
#else
#define check_uri_sanity(uri)
#endif

static inline struct uri_cache_entry *
get_uri_cache_entry(unsigned char *string, int length)
{
	struct uri_cache_entry *entry;
	struct hash_item *item;

	assert(string && length > 0);
	if_assert_failed return NULL;

	item = get_hash_item(uri_cache.map, string, length);
	if (item) return item->value;

	/* Setup a new entry */

	entry = mem_calloc(1, sizeof(struct uri_cache_entry) + length);
	if (!entry) return NULL;

	object_nolock(&entry->uri, "uri");
	memcpy(&entry->string, string, length);
	string = entry->string;

	if (parse_uri(&entry->uri, string) != URI_ERRNO_OK
	    || !add_hash_item(uri_cache.map, string, length, entry)) {
		mem_free(entry);
		return NULL;
	}

	object_lock(&uri_cache);

	return entry;
}

struct uri *
get_uri(unsigned char *string, int length)
{
	struct uri_cache_entry *entry;

	assert(string);

	if (length == -1) length = strlen(string);
	if (!length) return NULL;

	if (!is_object_used(&uri_cache)) {
		uri_cache.map = init_hash(hash_size(3), strhash);
		if (!uri_cache.map) return NULL;
		object_nolock(&uri_cache, "uri_cache");
	}

	entry = get_uri_cache_entry(string, length);
	if (!entry) {
		if (!is_object_used(&uri_cache))
			free_hash(uri_cache.map);
		return NULL;
	}

	check_uri_sanity(&entry->uri);
	object_nolock(&entry->uri, "uri");
	object_lock(&entry->uri);

	return &entry->uri;
}

void
done_uri(struct uri *uri)
{
	unsigned char *string = struri(uri);
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
