/* Connections managment */
/* $Id: connection.c,v 1.84 2003/07/06 11:53:47 jonas Exp $ */

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
#include "lowlevel/connect.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "lowlevel/ttime.h"
#include "protocol/protocol.h"
#include "protocol/proxy.h"
#include "protocol/uri.h"
#include "protocol/url.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "ssl/ssl.h"
#include "util/base64.h"
#include "util/encoding.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/types.h"


struct keepalive_connection {
	LIST_HEAD(struct keepalive_connection);

	protocol_handler *protocol;

	ttime timeout;
	ttime add_time;

	int port;
	int pf;
	int socket;

	unsigned char host[1]; /* Keep last */
};


static unsigned int connection_id = 0;
static int active_connections = 0;
static int keepalive_timeout = -1;

/* TODO: queue probably shouldn't be exported; ideally we should probably
 * separate it to an own module and define operations on it (especially
 * foreach_queue or so). Ok ok, that's nothing important and I'm not even
 * sure I would really like it ;-). --pasky */
INIT_LIST_HEAD(queue);
static INIT_LIST_HEAD(host_connections);
static INIT_LIST_HEAD(keepalive_connections);

/* Prototypes */
static void send_connection_info(struct connection *c);
static void check_keepalive_connections(void);

/* See connection_state description. */
#define is_in_result_state(cstate)	(cstate < 0)
#define is_in_progress_state(cstate)	(cstate >= 0)

static /* inline */ enum connection_priority
get_priority(struct connection *c)
{
	enum connection_priority priority;

	for (priority = 0; priority < PRIORITIES; priority++)
		if (c->pri[priority])
			break;

	assertm(priority != PRIORITIES, "Connection has no owner");

	return priority;
}

long
connect_info(int type)
{
	long info = 0;
	struct connection *ce;
	struct keepalive_connection *cee;

	switch (type) {
		case INFO_FILES:
			foreach (ce, queue) info++;
			break;
		case INFO_CONNECTING:
			foreach (ce, queue)
				info += (ce->state > S_WAIT && ce->state < S_TRANS);
			break;
		case INFO_TRANSFER:
			foreach (ce, queue) info += (ce->state == S_TRANS);
			break;
		case INFO_KEEP:
			foreach (cee, keepalive_connections) info++;
			break;
		default:
			internal("connect_info: bad request");
	}
	return info;
}

static inline int
connection_disappeared(struct connection *c)
{
	struct connection *d;

	foreach (d, queue)
		if (c == d && c->id == d->id)
			return 0;

	return 1;
}

/* Host connection management: */
/* Used to keep track on the number of connections to any given host. When
 * trying to setup a new connection the list is searched to see if the maximum
 * number of connection has been reached. If that is the case we try to suspend
 * an already established connection. */
/* Some connections (like file://) that do not involve hosts are not maintained
 * in the list. */

struct host_connection {
	LIST_HEAD(struct host_connection);

	int connections;
	unsigned char host[1]; /* Keep last */
};

static struct host_connection *
get_host_connection(struct connection *c)
{
	unsigned char *host = c->uri.host;
	int hostlen = c->uri.hostlen;
	struct host_connection *connection;

	if (!host) return NULL;
	foreach (connection, host_connections)
		if (!strncmp(connection->host, host, hostlen))
			return connection;

	return NULL;
}

/* Returns if the connection was succesfully added. */
/* Don't add hostnameless host connections but they're valid. */
static int
add_host_connection(struct connection *c)
{
	struct host_connection *hc = get_host_connection(c);

	if (hc) {
		hc->connections++;

	} else if (c->uri.host) {
		hc = mem_alloc(sizeof(struct host_connection) + c->uri.hostlen);
		if (!hc) return 0;

		hc->connections = 1;
		memcpy(hc->host, c->uri.host, c->uri.hostlen);
		add_to_list(host_connections, hc);
	}

	return 1;
}

/* Decrements and free()s the host connection if it is the last 'refcount'. */
static void
done_host_connection(struct connection *c)
{
	struct host_connection *h = get_host_connection(c);

	if (!h) return;

	h->connections--;
	if (h->connections > 0) return;

	del_from_list(h);
	mem_free(h);
}


#ifdef DEBUG
static void
check_queue_bugs(void)
{
	struct connection *d;
	enum connection_priority prev_priority = 0;
	int cc = 0;

	foreach (d, queue) {
		enum connection_priority priority = get_priority(d);

		assertm(priority >= prev_priority, "queue is not sorted");
		assertm(d->state >= 0, "interrupted connection on queue "
			"(conn %s, state %d)", d->uri.protocol, d->state);

		cc += d->running;
		prev_priority = priority;
	}

	assertm(cc == active_connections, "%d - %d", cc, active_connections);
}
#else
#define check_queue_bugs()
#endif


static struct connection *
init_connection(unsigned char *url, unsigned char *ref_url, int start,
		enum cache_mode cache_mode, enum connection_priority priority)
{
	struct connection *c = mem_calloc(1, sizeof(struct connection));

	if (!c) return NULL;

	c->uri.protocol = url;
	if (!parse_uri(&c->uri)) {
		/* Alert small hack to signal parse uri failure. */
		*url = 0;
		mem_free(c);
		return NULL;
	}

	c->id = connection_id++;
	c->ref_url = ref_url;
	c->pri[priority] =  1;
	c->cache_mode = cache_mode;
	c->sock1 = c->sock2 = -1;
	c->content_encoding = ENCODING_NONE;
	c->stream_pipes[0] = c->stream_pipes[1] = -1;
	init_list(c->downloads);
	c->est_length = -1;
	c->prg.start = start;
	c->prg.timer = -1;
	c->timer = -1;
	return c;
};

static void stat_timer(struct connection *c);

static void
update_remaining_info(struct connection *c)
{
	struct remaining_info *prg = &c->prg;
	ttime a = get_time() - prg->last_time;

	prg->loaded = c->received;
	prg->size = c->est_length;
	prg->pos = c->from;
	if (prg->size < prg->pos && prg->size != -1)
		prg->size = c->from;

	prg->dis_b += a;
	while (prg->dis_b >= SPD_DISP_TIME * CURRENT_SPD_SEC) {
		prg->cur_loaded -= prg->data_in_secs[0];
		memmove(prg->data_in_secs, prg->data_in_secs + 1,
			sizeof(int) * (CURRENT_SPD_SEC - 1));
		prg->data_in_secs[CURRENT_SPD_SEC - 1] = 0;
		prg->dis_b -= SPD_DISP_TIME;
	}

	prg->data_in_secs[CURRENT_SPD_SEC - 1] += prg->loaded - prg->last_loaded;
	prg->cur_loaded += prg->loaded - prg->last_loaded;
	prg->last_loaded = prg->loaded;
	prg->last_time += a;
	prg->elapsed += a;
	prg->timer = install_timer(SPD_DISP_TIME, (void (*)(void *)) stat_timer, c);
}

static void
stat_timer(struct connection *c)
{
	update_remaining_info(c);
	send_connection_info(c);
}

void
set_connection_state(struct connection *c, enum connection_state state)
{
	struct download *download;
	struct remaining_info *prg = &c->prg;

	if (is_in_result_state(c->state) && is_in_progress_state(state))
		c->prev_error = c->state;

	c->state = state;
	if (c->state == S_TRANS) {
		if (prg->timer == -1) {
			if (!prg->valid) {
				int tmp = prg->start;
				int tmp2 = prg->seek;

				memset(prg, 0, sizeof(struct remaining_info));
				prg->start = tmp;
				prg->seek = tmp2;
				prg->valid = 1;
			}
			prg->last_time = get_time();
			prg->last_loaded = prg->loaded;
			update_remaining_info(c);
			if (connection_disappeared(c))
				return;
		}

	} else if (prg->timer != -1) {
		kill_timer(prg->timer);
		prg->timer = -1;
	}

	foreach (download, c->downloads) {
		download->state = state;
		download->prev_error = c->prev_error;
	}

	if (is_in_progress_state(state)) send_connection_info(c);
}

static void
free_connection_data(struct connection *c)
{
	assertm(c->running, "connection already suspended");
	c->running = 0;

	if (c->sock1 != -1) set_handlers(c->sock1, NULL, NULL, NULL, NULL);
	if (c->sock2 != -1) set_handlers(c->sock2, NULL, NULL, NULL, NULL);
	close_socket(NULL, &c->sock2);

	/* XXX: See also protocol/http/http.c:uncompress_shutdown(). */
	if (c->stream) {
		close_encoded(c->stream);
		c->stream = NULL;
	}
	if (c->stream_pipes[1] >= 0)
		close(c->stream_pipes[1]);
	c->stream_pipes[0] = c->stream_pipes[1] = -1;

	if (c->dnsquery) {
		kill_dns_request(&c->dnsquery);
	}
	if (c->conn_info) {
		if (c->conn_info->addr) mem_free(c->conn_info->addr);
		mem_free(c->conn_info);
		c->conn_info = NULL;
	}
	if (c->buffer) {
		mem_free(c->buffer);
		c->buffer = NULL;
	}
	if (c->info) {
		mem_free(c->info);
		c->info = NULL;
	}
	if (c->timer != -1) {
		kill_timer(c->timer);
		c->timer = -1;
	}

	active_connections--;
	assertm(active_connections >= 0, "active connections underflow");

	if (c->state != S_WAIT)
		done_host_connection(c);
}

void
send_connection_info(struct connection *c)
{
	enum connection_state state = c->state;
	struct download *download = c->downloads.next;

	while ((void *)download != &c->downloads) {
		download->ce = c->cache;
		download = download->next;
		if (download->prev->end)
			download->prev->end(download->prev, download->prev->data);
		if (is_in_progress_state(state) && connection_disappeared(c))
			return;
	}
}

static void
done_connection(struct connection *c)
{
	del_from_list(c);
	send_connection_info(c);
	mem_free(c->uri.protocol);
	mem_free(c);
	check_queue_bugs();
}


static inline void
done_keepalive_connection(struct keepalive_connection *kc)
{
	del_from_list(kc);
	if (kc->socket != -1) close(kc->socket);
	mem_free(kc);
}

static struct keepalive_connection *
init_keepalive_connection(struct connection *c, ttime timeout)
{
	struct keepalive_connection *k;
	struct uri *uri = &c->uri;
	unsigned char *host = uri->user ? uri->user : uri->host;
	int hostlen  = (uri->port ? uri->port + uri->portlen - host
				  : uri->host + uri->hostlen - host);

	assertm(uri->host);

	k = mem_alloc(sizeof(struct keepalive_connection) + hostlen);
	if (!k) return NULL;

	memcpy(k->host, host, hostlen);
	k->port = get_uri_port(uri);
	k->protocol = get_protocol_handler(uri);
	k->pf = c->pf;
	k->socket = c->sock1;
	k->timeout = timeout;
	k->add_time = get_time();

	return k;
}

static struct keepalive_connection *
get_keepalive_connection(struct connection *c)
{
	struct keepalive_connection *connection;
	struct uri *uri = &c->uri;
	protocol_handler *handler = get_protocol_handler(uri);
	int port = get_uri_port(uri);
	unsigned char *host = uri->user ? uri->user : uri->host;
	int hostlen  = (uri->port ? uri->port + uri->portlen - host
				  : uri->host + uri->hostlen - host);

	if (!uri->host) return NULL;

	foreach (connection, keepalive_connections)
		if (connection->protocol == handler
		    && connection->port == port
		    && !strncmp(connection->host, host, hostlen))
			return connection;

	return NULL;
}

int
has_keepalive_connection(struct connection *c)
{
	struct keepalive_connection *k = get_keepalive_connection(c);

	if (!k) return 0;

	c->sock1 = k->socket;
	c->pf = k->pf;

	/* Mark that the socket should not be closed */
	k->socket = -1;
	done_keepalive_connection(k);

	return 1;
}

void
add_keepalive_connection(struct connection *c, ttime timeout)
{
	struct keepalive_connection *k;

	free_connection_data(c);
	assertm(c->sock1 != -1, "keepalive connection not connected");

	k = init_keepalive_connection(c, timeout);
	if (k)
		add_to_list(keepalive_connections, k);
	else
		close(c->sock1);

	done_connection(c);
	register_bottom_half((void (*)(void *))check_queue, NULL);
}

static void
keepalive_timer(void *x)
{
	keepalive_timeout = -1;
	check_keepalive_connections();
}

void
check_keepalive_connections(void)
{
	struct keepalive_connection *kc;
	ttime ct = get_time();
	int p = 0;

	if (keepalive_timeout != -1) {
		kill_timer(keepalive_timeout);
		keepalive_timeout = -1;
	}

	foreach (kc, keepalive_connections) {
		if (can_read(kc->socket) || ct - kc->add_time > kc->timeout) {
			kc = kc->prev;
			done_keepalive_connection(kc->next);
		} else {
			p++;
		}
	}

	for (; p > MAX_KEEPALIVE_CONNECTIONS; p--) {
		assert(!list_empty(keepalive_connections));
		done_keepalive_connection(keepalive_connections.prev);
	}

	if (!list_empty(keepalive_connections))
		keepalive_timeout = install_timer(KEEPALIVE_CHECK_TIME,
						  keepalive_timer, NULL);
}

static inline void
abort_all_keepalive_connections(void)
{
	while (!list_empty(keepalive_connections))
		done_keepalive_connection(keepalive_connections.next);

	check_keepalive_connections();
}


static inline void
add_to_queue(struct connection *c)
{
	struct connection *cc;
	enum connection_priority priority = get_priority(c);

	foreach (cc, queue)
		if (get_priority(cc) > priority)
			break;

	add_at_pos(cc->prev, c);
}

static void
sort_queue(void)
{
	struct connection *c;
	int swp;

	do {
		swp = 0;
		foreach (c, queue) {
			if ((void *)c->next == &queue) break;

			if (get_priority(c->next) < get_priority(c)) {
				struct connection *n = c->next;

				del_from_list(c);
				add_at_pos(n, c);
				swp = 1;
			}
		}
	} while (swp);
}

static void
interrupt_connection(struct connection *c)
{
	if (c->ssl) {
		free_ssl(c->ssl);
		c->ssl = NULL;
	}

	close_socket(c, &c->sock1);
	free_connection_data(c);
}

static inline void
suspend_connection(struct connection *c)
{
	interrupt_connection(c);
	set_connection_state(c, S_WAIT);
}

static void
run_connection(struct connection *c)
{
	protocol_handler *func = get_protocol_handler(&c->uri);

	assertm(!c->running, "connection already running");

	if (!add_host_connection(c)) {
		set_connection_state(c, S_OUT_OF_MEM);
		done_connection(c);
		return;
	}

	active_connections++;
	c->running = 1;
	func(c);
}

void
retry_connection(struct connection *c)
{
	int max_tries = get_opt_int("connection.retries");

	interrupt_connection(c);
	if (c->uri.post || !max_tries || ++c->tries >= max_tries) {
		/*send_connection_info(c);*/
		done_connection(c);
		register_bottom_half((void (*)(void *))check_queue, NULL);
	} else {
		c->prev_error = c->state;
		run_connection(c);
	}
}

void
abort_connection(struct connection *c)
{
	if (c->running) interrupt_connection(c);
	/* send_connection_info(c); */
	done_connection(c);
	register_bottom_half((void (*)(void *))check_queue, NULL);
}

/* Set certain state on a connection and then abort the connection. */
void
abort_conn_with_state(struct connection *conn, enum connection_state state)
{
	set_connection_state(conn, state);
	abort_connection(conn);
}

/* Set certain state on a connection and then retry the connection. */
void
retry_conn_with_state(struct connection *conn, enum connection_state state)
{
	set_connection_state(conn, state);
	retry_connection(conn);
}

static int
try_to_suspend_connection(struct connection *c, unsigned char *ho)
{
	enum connection_priority priority = get_priority(c);
	struct connection *d;

	foreachback (d, queue) {
		if (get_priority(d) <= priority) return -1;
		if (d->state == S_WAIT) continue;
		if (d->uri.post && get_priority(d) < PRI_CANCEL) continue;
		if (ho && strncmp(c->uri.host, ho, c->uri.hostlen)) continue;
		suspend_connection(d);
		return 0;
	}

	return -1;
}

static inline int
try_connection(struct connection *c, int max_conns_to_host, int max_conns)
{
	struct host_connection *hc = get_host_connection(c);

	if (hc && hc->connections >= max_conns_to_host)
		return try_to_suspend_connection(c, hc->host) ? 0 : -1;

	if (active_connections >= max_conns)
		return try_to_suspend_connection(c, NULL) ? 0 : -1;

	run_connection(c);
	return 1;
}

void
check_queue(void)
{
	struct connection *c;
	int max_conns_to_host = get_opt_int("connection.max_connections_to_host");
	int max_conns = get_opt_int("connection.max_connections");

again:
	c = queue.next;
	check_queue_bugs();
	check_keepalive_connections();

	while (c != (struct connection *)&queue) {
		struct connection *d;
		enum connection_priority cp = get_priority(c);

		/* No way to reduce code redundancy here ? --Zas */
		for (d = c; d != (struct connection *)&queue && get_priority(d) == cp;) {
			struct connection *dd = d;

			d = d->next;
			if (dd->state == S_WAIT && get_keepalive_connection(dd)
			    && try_connection(dd, max_conns_to_host, max_conns))
				goto again;
		}

		for (d = c; d != (struct connection *)&queue && get_priority(d) == cp;) {
			struct connection *dd = d;

			d = d->next;
			if (dd->state == S_WAIT
			    && try_connection(dd, max_conns_to_host, max_conns))
				goto again;
		}
		c = d;
	}

again2:
	foreachback (c, queue) {
		if (get_priority(c) < PRI_CANCEL) break;
		if (c->state == S_WAIT) {
			set_connection_state(c, S_INTERRUPTED);
			done_connection(c);
			goto again2;
		}
	}

	check_queue_bugs();
}

int
load_url(unsigned char *url, unsigned char *ref_url, struct download *download,
	 enum connection_priority pri, enum cache_mode cache_mode, int start)
{
	struct cache_entry *e = NULL;
	struct connection *c;
	unsigned char *u;

	if (download) {
		download->c = NULL;
		download->ce = NULL;
		download->pri = pri;
		download->state = S_OUT_OF_MEM;
		download->prev_error = 0;
	}

#ifdef DEBUG
	foreach (c, queue) {
		struct download *assigned;

		foreach (assigned, c->downloads)
			assert(assigned != download);
	}
#endif

	if (cache_mode <= NC_CACHE && find_in_cache(url, &e) && !e->incomplete) {
		if (!e->refcount &&
		    ((e->cache_mode == NC_PR_NO_CACHE && cache_mode != NC_ALWAYS_CACHE)
		     || (e->redirect && !get_opt_int("document.cache.cache_redirects")))) {
			delete_cache_entry(e);
			e = NULL;
		} else {
			if (download) {
				download->ce = e;
				download->state = S_OK;
			/* XXX: This doesn't work since sometimes stat->prg is
			 * undefined and contains random memory locations. It's
			 * not supposed to point on anything here since stat
			 * has no connection attached. Downloads resuming will
			 * probably break in some cases without this, though.
			 * FIXME: Needs more investigation. --pasky */
			/* if (stat->prg) stat->prg->start = start; */
				if (download->end)
					download->end(download, download->data);
			}
			return 0;
		}
	}

	u = get_proxy(url);
	if (!u) {
		if (download) download->end(download, download->data);
		return -1;
	}

	foreach (c, queue) {
		if (c->detached || strcmp(c->uri.protocol, u))
			continue;

		mem_free(u);

		if (get_priority(c) > pri) {
			del_from_list(c);
			c->pri[pri]++;
			add_to_queue(c);
			register_bottom_half((void (*)(void *))check_queue, NULL);
		} else {
			c->pri[pri]++;
		}

		if (download) {
			download->prg = &c->prg;
			download->c = c;
			download->ce = c->cache;
			add_to_list(c->downloads, download);
			set_connection_state(c, c->state);
		}
		check_queue_bugs();
		return 0;
	}

	assert(!e);
	c = init_connection(u, ref_url, start, cache_mode, pri);
	if (!c) {
		if (download) {
			/* Zero length uri signals parse uri failure */
			download->state = (!*u ? S_BAD_URL : S_OUT_OF_MEM);
			download->end(download, download->data);
		}
		mem_free(u);
		return -1;
	}

	if (download) {
		download->prg = &c->prg;
		download->c = c;
		download->ce = NULL;
		add_to_list(c->downloads, download);
	}

	add_to_queue(c);
	set_connection_state(c, S_WAIT);

	check_queue_bugs();

	register_bottom_half((void (*)(void *))check_queue, NULL);
	return 0;
}


/* FIXME: one object in more connections */
void
change_connection(struct download *old, struct download *new,
		  int newpri, int interrupt)
{
	struct connection *c;

	assert(old);

	if (is_in_result_state(old->state)) {
		if (new) {
			new->ce = old->ce;
			new->state = old->state;
			new->prev_error = old->prev_error;
			if (new->end) new->end(new, new->data);
		}
		return;
	}

	check_queue_bugs();

	c = old->c;

	c->pri[old->pri]--;
	assertm(c->pri[old->pri] >= 0, "priority counter underflow");

	c->pri[newpri]++;
	del_from_list(old);
	old->state = S_INTERRUPTED;

	if (new) {
		new->prg = &c->prg;
		add_to_list(c->downloads, new);
		new->state = c->state;
		new->prev_error = c->prev_error;
		new->pri = newpri;
		new->c = c;
		new->ce = c->cache;

	} else if (c->detached || interrupt) {
		abort_conn_with_state(c, S_INTERRUPTED);
	}

	sort_queue();
	check_queue_bugs();

	register_bottom_half((void (*)(void *))check_queue, NULL);
}

/* This will remove 'pos' bytes from the start of the cache for the specified
 * connection, if the cached object is already too big. */
void
detach_connection(struct download *download, int pos)
{
	struct connection *conn = download->c;

	if (is_in_result_state(download->state)) return;

	if (!conn->detached) {
		int total_len;
		int i, total_pri = 0;

		if (!conn->cache)
			return;

		total_len = (conn->est_length == -1) ? conn->from
						     : conn->est_length;

		if (total_len < (get_opt_long("document.cache.memory.size")
				 * MAX_CACHED_OBJECT_PERCENT / 100)) {
			/* This whole thing will fit to the memory anyway, so
			 * there's no problem in detaching the connection. */
			return;
		}

		for (i = 0; i < PRI_CANCEL; i++)
			total_pri += conn->pri[i];
		assertm(total_pri, "detaching free connection");

		/* Pre-clean cache. */
		delete_unused_format_cache_entries();

		if (total_pri != 1 || conn->cache->refcount) {
			/* We're too important, or someone uses our cache
			 * entry. */
			return;
		}

		/* debug("detached"); */

		/* We aren't valid cache entry anymore. */
		conn->cache->url[0] = 0;
		conn->detached = 1;
	}

	/* Strip the entry. */
	free_entry_to(conn->cache, pos);
}

static void
connection_timeout(struct connection *c)
{
	c->timer = -1;
	set_connection_state(c, S_TIMEOUT);
	if (c->dnsquery) {
		abort_connection(c);
	} else if (c->conn_info) {
		dns_found(c, 0); /* jump to next addr */
		if (c->conn_info) set_connection_timeout(c);
	} else {
		retry_connection(c);
	}
}

/* Huh, using two timers? Is this to account for changes of c->unrestartable
 * or can it be reduced? --jonas */
static void
connection_timeout_1(struct connection *c)
{
	c->timer = install_timer((c->unrestartable ? get_opt_int("connection.unrestartable_receive_timeout")
						   : get_opt_int("connection.receive_timeout"))
				 * 500, (void (*)(void *)) connection_timeout, c);
}

void
set_connection_timeout(struct connection *c)
{
	if (c->timer != -1) kill_timer(c->timer);
	c->timer = install_timer((c->unrestartable ? get_opt_int("connection.unrestartable_receive_timeout")
						   : get_opt_int("connection.receive_timeout"))
				 * 500, (void (*)(void *))connection_timeout_1, c);
}


void
abort_all_connections(void)
{
	while (!list_empty(queue)) {
		set_connection_state(queue.next, S_INTERRUPTED);
		abort_connection(queue.next);
	}

	abort_all_keepalive_connections();
}

void
abort_background_connections(void)
{
	int i = 0;

	while (1) {
		int j;
		struct connection *c = (void *)&queue;

		for (j = 0; j <= i; j++) {
			c = c->next;
			if (c == (void *) &queue)
				return;
		}

		if (get_priority(c) >= PRI_CANCEL)
			abort_conn_with_state(c, S_INTERRUPTED);
		else
			i++;
	}
}
