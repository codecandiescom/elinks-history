/* URL parser and translator; implementation of RFC 2396. */
/* $Id: uri.c,v 1.1 2003/07/01 15:22:39 jonas Exp $ */

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
#include "protocol/url.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


int
parse_uri(unsigned char *url, int *prlen,
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
	int protocol;

	assertm(url, "No url to parse.");

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

	if (!*url) return -1; /* Empty url. */

	/* Isolate prefix */

	prefix_end = strchr(url, ':');
	if (!prefix_end) return -1;

	if (prlen) *prlen = prefix_end - url;

	/* Get protocol */

	protocol = check_protocol(url, prefix_end - url);
	if (protocol == PROTOCOL_UNKNOWN) return -1;

	prefix_end++; /* ':' */

	/* Skip slashes */

	if (prefix_end[0] == '/' && prefix_end[1] == '/')
		prefix_end += 2;
	else if (get_protocol_need_slashes(protocol))
		return -1;

	if (get_protocol_free_syntax(protocol)) {
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

	if (!*host_end && get_protocol_need_slash_after_host(protocol))
		return -1;

	if (host || holen) { /* Only enter if needed. */
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

		host_end++;

		if (port) *port = host_end;
		if (polen) *polen = port_end - host_end;

		/* test if port is number */
		/* TODO: possibly lookup for the service otherwise? --pasky */
		for (; host_end < port_end; host_end++)
			if (*host_end < '0' || *host_end > '9')
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
