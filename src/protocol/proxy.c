/* Proxy handling */
/* $Id: proxy.c,v 1.42 2004/07/22 06:09:35 miciah Exp $ */

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

/* TODO: We could of course significantly simplify the calling convention by
 * autogenerating most of the parameters from protocol name. Having a function
 * exported by protocol/protocol.* dedicated to that would be nice too.
 * --pasky */
static unsigned char *
get_protocol_proxy(unsigned char *opt,
                   unsigned char *env1, unsigned char *env2,
		   unsigned char *strip1, unsigned char *strip2)
{
	unsigned char *proxy;

	proxy = get_opt_str(opt);
	if (!*proxy) proxy = getenv(env1);
	if (!proxy || !*proxy) proxy = getenv(env2);

	if (proxy && *proxy) {
		if (!strncasecmp(proxy, strip1, strlen(strip1)))
			proxy += strlen(strip1);
		else if (strip2 && !strncasecmp(proxy, strip2, strlen(strip2)))
			proxy += strlen(strip2);
	}

	return proxy;
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
		protocol_proxy = get_protocol_proxy("protocol.http.proxy.host",
						    "HTTP_PROXY", "http_proxy",
						    "http://", NULL);
		break;

	case PROTOCOL_HTTPS:
		protocol_proxy = get_protocol_proxy("protocol.https.proxy.host",
						    "HTTPS_PROXY", "https_proxy",
						    "https://", NULL);
		break;

	case PROTOCOL_FTP:
		protocol_proxy = get_protocol_proxy("protocol.ftp.proxy.host",
						    "FTP_PROXY", "ftp_proxy",
						    "ftp://", "http://");
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
			return proxy_uri(uri, protocol_proxy);
	}

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
