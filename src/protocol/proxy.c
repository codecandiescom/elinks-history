/* Proxy handling */
/* $Id: proxy.c,v 1.38 2004/07/22 00:00:47 pasky Exp $ */

#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/event.h"
#include "util/memory.h"
#include "util/string.h"


static int
proxy_probe_no_proxy(unsigned char *url, unsigned char *no_proxy)
{
	unsigned char *slash = strchr(url, '/');

	if (slash) *slash = '\0';

	while (no_proxy && *no_proxy) {
		unsigned char *jumper = strchr(no_proxy, ',');

		skip_space(no_proxy);
		if (jumper) *jumper = '\0';

		if (strcasestr(url, no_proxy)) {
			if (jumper) *jumper = ',';
			if (slash) *slash = '/';
			return 1;
		}
		no_proxy = jumper;
		if (jumper) {
			*jumper = ',';
			no_proxy++;
		}
	}

	if (slash) *slash = '/';
	return 0;
}

static struct uri *
proxy_uri(struct uri *uri, unsigned char *proxy)
{
	struct string string;

	if (init_string(&string)
	    && string_concat(&string, "proxy://", proxy, "/", NULL)
	    && add_uri_to_string(&string, uri, URI_PROXY)) {
		/* There is no need to use URI_BASE here since URI_PROXY should
		 * not add any fragments in the first place. */
		uri = get_uri(string.source, 0);
	} else {
		uri = NULL;
	}

	done_string(&string);
	return uri;
}

static struct uri *
get_proxy_worker(struct uri *uri, unsigned char *proxy)
{
	unsigned char *protocol_proxy = NULL;

	if (proxy) {
		if (*proxy)
			return proxy_uri(uri, proxy);

		/* "" from script_hook_get_proxy() */
		return get_composed_uri(uri, URI_BASE);
	}

	switch (uri->protocol) {
	case PROTOCOL_HTTP:
		protocol_proxy = get_opt_str("protocol.http.proxy.host");
		if (!*protocol_proxy) protocol_proxy = getenv("HTTP_PROXY");
		if (!protocol_proxy || !*protocol_proxy) protocol_proxy = getenv("protocol_proxy");

		if (protocol_proxy && *protocol_proxy) {
			if (!strncasecmp(protocol_proxy, "http://", 7))
				protocol_proxy += 7;
		}
		break;

	case PROTOCOL_HTTPS:
		protocol_proxy = get_opt_str("protocol.https.proxy.host");
		if (!*protocol_proxy) protocol_proxy = getenv("HTTPS_PROXY");
		if (!protocol_proxy || !*protocol_proxy) protocol_proxy = getenv("protocol_proxy");

		if (protocol_proxy && *protocol_proxy) {
			if (!strncasecmp(protocol_proxy, "http://", 7))
				protocol_proxy += 7;
		}
		break;

	case PROTOCOL_FTP:
		protocol_proxy = get_opt_str("protocol.ftp.proxy.host");
		if (!*protocol_proxy) protocol_proxy = getenv("FTP_PROXY");
		if (!protocol_proxy || !*protocol_proxy) protocol_proxy = getenv("protocol_proxy");

		if (protocol_proxy && *protocol_proxy) {
			if (!strncasecmp(protocol_proxy, "ftp://", 6))
				protocol_proxy += 6;
			else if (!strncasecmp(protocol_proxy, "http://", 7))
				protocol_proxy += 7;
		}
		break;
	}

	if (protocol_proxy && *protocol_proxy) {
		unsigned char *no_proxy;
		unsigned char *slash = strchr(protocol_proxy, '/');

		if (slash) *slash = 0;

		no_proxy = get_opt_str("protocol.no_proxy");
		if (!*no_proxy) no_proxy = getenv("NO_PROXY");
		if (!no_proxy || !*no_proxy) no_proxy = getenv("no_proxy");

		if (!proxy_probe_no_proxy(uri->host, no_proxy))
			proxy = protocol_proxy;
	}

	if (proxy)
		return proxy_uri(uri, proxy);

	return get_composed_uri(uri, URI_BASE);
}

struct uri *
get_proxy_uri(struct uri *uri)
{
	if (uri->protocol == PROTOCOL_PROXY) {
		return get_composed_uri(uri, URI_BASE);
	} else {
#ifdef CONFIG_SCRIPTING
		unsigned char *tmp = NULL;
		static int get_proxy_event_id = EVENT_NONE;

		set_event_id(get_proxy_event_id, "get-proxy");
		trigger_event(get_proxy_event_id, &tmp, struri(uri));
		uri = get_proxy_worker(uri, tmp);
		mem_free_if(tmp);
		return uri;
#else
		return get_proxy_worker(uri, NULL);
#endif
	}
}

struct uri *
get_proxied_uri(struct uri *uri)
{
	if (uri->protocol == PROTOCOL_PROXY)
		return get_uri(uri->data, URI_BASE);

	return get_composed_uri(uri, URI_BASE);
}
