/* Proxy handling */
/* $Id: proxy.c,v 1.10 2004/04/01 06:22:38 jonas Exp $ */

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

		if (jumper) *jumper = '\0';
		if (strstr(url, no_proxy)) {
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

static unsigned char *
get_proxy_worker(struct uri *uri, unsigned char *proxy)
{
	unsigned char *http_proxy, *https_proxy, *ftp_proxy, *no_proxy;

	http_proxy = get_opt_str("protocol.http.proxy.host");
	if (!*http_proxy) http_proxy = getenv("HTTP_PROXY");
	if (!http_proxy || !*http_proxy) http_proxy = getenv("http_proxy");

	https_proxy = get_opt_str("protocol.https.proxy.host");
	if (!*https_proxy) https_proxy = getenv("HTTPS_PROXY");
	if (!https_proxy || !*https_proxy) https_proxy = getenv("https_proxy");

	ftp_proxy = get_opt_str("protocol.ftp.proxy.host");
	if (!*ftp_proxy) ftp_proxy = getenv("FTP_PROXY");
	if (!ftp_proxy || !*ftp_proxy) ftp_proxy = getenv("ftp_proxy");

	no_proxy = get_opt_str("protocol.no_proxy");
	if (!*no_proxy) no_proxy = getenv("NO_PROXY");
	if (!no_proxy || !*no_proxy) no_proxy = getenv("no_proxy");

	if (proxy) {
		if (!*proxy) proxy = NULL; /* "" from script_hook_get_proxy() */
	} else {
		unsigned char *slash;

		if (http_proxy && *http_proxy && uri->protocol == PROTOCOL_HTTP) {
			if (!strncasecmp(http_proxy, "http://", 7))
				http_proxy += 7;

			slash = strchr(http_proxy, '/');
			if (slash) *slash = 0;

			if (!proxy_probe_no_proxy(uri->host, no_proxy))
				proxy = http_proxy;
		}

		if (https_proxy && *https_proxy && uri->protocol == PROTOCOL_HTTPS) {
			if (!strncasecmp(https_proxy, "http://", 7))
				https_proxy += 7;

			slash = strchr(https_proxy, '/');
			if (slash) *slash = 0;

			if (!proxy_probe_no_proxy(uri->host, no_proxy))
				proxy = https_proxy;
		}

		if (ftp_proxy && *ftp_proxy && uri->protocol == PROTOCOL_FTP) {
			if (!strncasecmp(ftp_proxy, "ftp://", 6))
				ftp_proxy += 6;
			else if(!strncasecmp(ftp_proxy, "http://", 7))
				ftp_proxy += 7;

			slash = strchr(ftp_proxy, '/');
			if (slash) *slash = 0;

			if (!proxy_probe_no_proxy(uri->host, no_proxy))
				proxy = ftp_proxy;
		}
	}

	if (proxy) {
		return straconcat("proxy://", proxy, "/", struri(uri), NULL);
	}

	return stracpy(struri(uri));
}

unsigned char *
get_proxy(struct uri *uri)
{
#ifdef HAVE_SCRIPTING
	unsigned char *tmp = NULL;
	unsigned char *ret;
	static int get_proxy_event_id = EVENT_NONE;

	set_event_id(get_proxy_event_id, "get-proxy");
	trigger_event(get_proxy_event_id, &tmp, struri(uri));
	ret = get_proxy_worker(uri, tmp);
	if (tmp) mem_free(tmp);
	return ret;
#else
	return get_proxy_worker(uri, NULL);
#endif
}
