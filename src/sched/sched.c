/* Connections startup */
/* $Id: sched.c,v 1.31 2003/07/02 23:40:04 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "document/cache.h"
#include "document/html/renderer.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "protocol/protocol.h"
#include "protocol/url.h"
#include "sched/connection.h"
#include "sched/sched.h"
#include "sched/session.h"
#include "scripting/lua/hooks.h"
#include "util/base64.h"
#include "util/encoding.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


static tcount connection_count = 0;


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

static unsigned char *
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

/* Note that stat's data _MUST_ be struct download * if start > 0! Yes, that
 * should be probably something else than data, but... ;-) */
int
load_url(unsigned char *url, unsigned char *ref_url,
	 struct status *stat, int pri, enum cache_mode cache_mode, int start)
{
	struct cache_entry *e = NULL;
	struct connection *c;
	unsigned char *u;

	if (stat) {
		stat->c = NULL;
		stat->ce = NULL;
		stat->pri = pri;
	}

#ifdef DEBUG
	foreach (c, queue) {
		struct status *st;

		foreach (st, c->statuss) {
			if (st == stat) {
				internal("status already assigned to '%s'", c->url);
				stat->state = S_INTERNAL;
				if (stat->end) stat->end(stat, stat->data);
				return 0;
			}
		}
	}
#endif

	if (stat) {
		stat->state = S_OUT_OF_MEM;
		stat->prev_error = 0;
	}

	if (cache_mode <= NC_CACHE && find_in_cache(url, &e) && !e->incomplete) {
		if (!e->refcount &&
		    ((e->cache_mode == NC_PR_NO_CACHE && cache_mode != NC_ALWAYS_CACHE)
		     || (e->redirect && !get_opt_int("document.cache.cache_redirects")))) {
			delete_cache_entry(e);
			e = NULL;
		} else {
			if (stat) {
				stat->ce = e;
				stat->state = S_OK;
			/* XXX: This doesn't work since sometimes stat->prg is
			 * undefined and contains random memory locations. It's
			 * not supposed to point on anything here since stat
			 * has no connection attached. Downloads resuming will
			 * probably break in some cases without this, though.
			 * FIXME: Needs more investigation. --pasky */
			/* if (stat->prg) stat->prg->start = start; */
				if (stat->end) stat->end(stat, stat->data);
			}
			return 0;
		}
	}

	u = get_proxy(url);
	if (!u) {
		if (stat) stat->end(stat, stat->data);
		return -1;
	}

	foreach (c, queue) {
		if (c->detached || strcmp(c->url, u))
			continue;

		mem_free(u);

		if (getpri(c) > pri) {
			del_from_list(c);
			c->pri[pri]++;
			add_to_queue(c);
			register_bottom_half((void (*)(void *))check_queue, NULL);
		} else {
			c->pri[pri]++;
		}

		if (stat) {
			stat->prg = &c->prg;
			stat->c = c;
			stat->ce = c->cache;
			add_to_list(c->statuss, stat);
			setcstate(c, c->state);
		}
#ifdef DEBUG
		check_queue_bugs();
#endif
		return 0;
	}

	c = mem_calloc(1, sizeof(struct connection));
	if (!c) {
		if (stat) stat->end(stat, stat->data);
		mem_free(u);
		return -1;
	}

	c->count = connection_count++;
	c->url = u;
	c->ref_url = ref_url;

	if (cache_mode < NC_RELOAD && e && e->frag.next != &e->frag
	    && !((struct fragment *) e->frag.next)->offset)
		c->from = ((struct fragment *) e->frag.next)->length;

	c->pri[pri] = 1;
	c->cache_mode = cache_mode;
	c->sock1 = c->sock2 = -1;
	c->content_encoding = ENCODING_NONE;
	c->stream_pipes[0] = c->stream_pipes[1] = -1;
	init_list(c->statuss);
	c->est_length = -1;
	c->prg.start = start;
	c->prg.timer = -1;
	c->timer = -1;

	if (stat) {
		stat->prg = &c->prg;
		stat->c = c;
		stat->ce = NULL;
		add_to_list(c->statuss, stat);
	}

	add_to_queue(c);
	setcstate(c, S_WAIT);

#ifdef DEBUG
	check_queue_bugs();
#endif

	register_bottom_half((void (*)(void *))check_queue, NULL);
	return 0;
}

/* FIXME: trash it ? --Zas */
#if 0
/* Not used for now. */
void
reset_timeout(struct connection *c)
{
	if (c->timer != -1) {
		kill_timer(c->timer);
		c->timer = -1;
	}
}
#endif
