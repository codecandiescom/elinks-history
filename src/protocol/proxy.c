/* Proxy handling */
/* $Id: proxy.c,v 1.3 2003/07/24 15:33:32 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "scripting/guile/hooks.h"
#include "scripting/lua/hooks.h"
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
get_proxy_worker(unsigned char *url, unsigned char *proxy)
{
	int l = strlen(url);
	unsigned char *http_proxy, *ftp_proxy, *no_proxy;

	http_proxy = get_opt_str("protocol.http.proxy.host");
	if (!*http_proxy) http_proxy = getenv("HTTP_PROXY");
	if (!http_proxy || !*http_proxy) http_proxy = getenv("http_proxy");

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

		if (http_proxy && *http_proxy) {
			if (!strncasecmp(http_proxy, "http://", 7))
				http_proxy += 7;

			slash = strchr(http_proxy, '/');
			if (slash) *slash = 0;

			if (l >= 7 && !strncasecmp(url, "http://", 7)
			    && !proxy_probe_no_proxy(url + 7, no_proxy))
				proxy = http_proxy;
		}

		if (ftp_proxy && *ftp_proxy) {
			if (!strncasecmp(ftp_proxy, "ftp://", 6))
				ftp_proxy += 6;

			slash = strchr(ftp_proxy, '/');
			if (slash) *slash = 0;

			if (l >= 6 && !strncasecmp(url, "ftp://", 6)
			    && !proxy_probe_no_proxy(url + 6, no_proxy))
				proxy = ftp_proxy;
		}
	}

	if (proxy) {
		return straconcat("proxy://", proxy, "/", url, NULL);
	}

	return stracpy(url);
}

unsigned char *
get_proxy(unsigned char *url)
{
#ifdef HAVE_SCRIPTING
	unsigned char *tmp = script_hook_get_proxy(url);
	unsigned char *ret = get_proxy_worker(url, tmp);

	if (tmp) mem_free(tmp);
	return ret;
#else
	return get_proxy_worker(url, NULL);
#endif
}
