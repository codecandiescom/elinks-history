/* URL parser and translator; implementation of RFC 2396. */
/* $Id: uri.c,v 1.9 2003/07/12 20:21:15 jonas Exp $ */

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

unsigned char *
get_uri_string(struct uri *uri)
{
	unsigned char *str = init_str();
	int len = 0;

	if (!str) return NULL;
	assert(uri->protocol && uri->protocollen && uri->host && uri->hostlen);
	if_assert_failed { mem_free(str); return NULL; }

	add_bytes_to_str(&str, &len, uri->protocol, uri->protocollen);
	add_to_str(&str, &len, "://");
	add_bytes_to_str(&str, &len, uri->host, uri->hostlen);
	add_chr_to_str(&str, &len, ':');

	if (uri->port && uri->portlen) {
		add_bytes_to_str(&str, &len, uri->port, uri->portlen);
	} else {
		/* Should user protocols ports be configurable? */
		enum protocol protocol = check_protocol(uri->protocol,
							uri->protocollen);
		int port = get_protocol_port(protocol);

		/* RFC2616 section 3.2.2:
		 * "If the port is empty or not given, port 80 is assumed." */
		/* Port 0 comes from user protocol backend so be httpcentric. */
		add_num_to_str(&str, &len, (port != 0 ? port : 80));
	}

	return str;
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
encode_uri_string(unsigned char *name, unsigned char **data, int *len)
{
	unsigned char n[4];

	n[0] = '%';
	n[3] = '\0';

	for (; *name; name++) {
#if 0
		/* This is probably correct only for query part of URI..? */
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
