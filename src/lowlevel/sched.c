/* Connections managment */
/* $Id: sched.c,v 1.42 2002/09/12 12:37:59 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "links.h"

#include "config/options.h"
#include "document/cache.h"
#include "document/session.h"
#include "document/html/renderer.h"
#include "intl/language.h"
#include "lowlevel/connect.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "lowlevel/ttime.h"
#include "lua/hooks.h"
#include "protocol/url.h"
#include "ssl/ssl.h"
#include "util/base64.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* Types and structs */
struct list_head queue = {&queue, &queue};

struct h_conn {
	struct h_conn *next;
	struct h_conn *prev;
	unsigned char *host;
	int conn;
};

struct k_conn {
	struct k_conn *next;
	struct k_conn *prev;
	void (*protocol)(struct connection *);
	unsigned char *host;
	int port;
	int pf;
	int conn;
	ttime timeout;
	ttime add_time;
};

/* Global variables */
tcount connection_count = 0;
int active_connections = 0;
int st_r = 0;
int keepalive_timeout = -1;

struct list_head h_conns = {&h_conns, &h_conns};
struct list_head keepalive_connections = {&keepalive_connections, &keepalive_connections};

struct s_msg_dsc msg_dsc[] = {
	{S_WAIT,		TEXT(T_WAITING_IN_QUEUE)},
	{S_DNS,			TEXT(T_LOOKING_UP_HOST)},
	{S_CONN,		TEXT(T_MAKING_CONNECTION)},
	{S_SSL_NEG,		TEXT(T_SSL_NEGOTIATION)},
	{S_SENT,		TEXT(T_REQUEST_SENT)},
	{S_LOGIN,		TEXT(T_LOGGING_IN)},
	{S_GETH,		TEXT(T_GETTING_HEADERS)},
	{S_PROC,		TEXT(T_SERVER_IS_PROCESSING_REQUEST)},
	{S_TRANS,		TEXT(T_TRANSFERRING)},

	{S_WAIT_REDIR,		TEXT(T_WAITING_FOR_REDIRECT_CONFIRMATION)},
	{S_OK,			TEXT(T_OK)},
	{S_INTERRUPTED,		TEXT(T_INTERRUPTED)},
	{S_EXCEPT,		TEXT(T_SOCKET_EXCEPTION)},
	{S_INTERNAL,		TEXT(T_INTERNAL_ERROR)},
	{S_OUT_OF_MEM,		TEXT(T_OUT_OF_MEMORY)},
	{S_NO_DNS,		TEXT(T_HOST_NOT_FOUND)},
	{S_CANT_WRITE,		TEXT(T_ERROR_WRITING_TO_SOCKET)},
	{S_CANT_READ,		TEXT(T_ERROR_READING_FROM_SOCKET)},
	{S_MODIFIED,		TEXT(T_DATA_MODIFIED)},
	{S_BAD_URL,		TEXT(T_BAD_URL_SYNTAX)},
	{S_TIMEOUT,		TEXT(T_RECEIVE_TIMEOUT)},
	{S_RESTART,		TEXT(T_REQUEST_MUST_BE_RESTARTED)},
	{S_STATE,		TEXT(T_CANT_GET_SOCKET_STATE)},

	{S_HTTP_ERROR,		TEXT(T_BAD_HTTP_RESPONSE)},
	{S_HTTP_100,		TEXT(T_HTTP_100)},
	{S_HTTP_204,		TEXT(T_NO_CONTENT)},

	{S_FILE_TYPE,		TEXT(T_UNKNOWN_FILE_TYPE)},
	{S_FILE_ERROR,		TEXT(T_ERROR_OPENING_FILE)},

	{S_FTP_ERROR,		TEXT(T_BAD_FTP_RESPONSE)},
	{S_FTP_UNAVAIL,		TEXT(T_FTP_SERVICE_UNAVAILABLE)},
	{S_FTP_LOGIN,		TEXT(T_BAD_FTP_LOGIN)},
	{S_FTP_PORT,		TEXT(T_FTP_PORT_COMMAND_FAILED)},
	{S_FTP_NO_FILE,		TEXT(T_FILE_NOT_FOUND)},
	{S_FTP_FILE_ERROR,	TEXT(T_FTP_FILE_ERROR)},
#ifdef HAVE_SSL
	{S_SSL_ERROR,		TEXT(T_SSL_ERROR)},
#else
	{S_NO_SSL,		TEXT(T_NO_SSL)},
#endif
	{0,			NULL}
};

/* Prototypes */
void send_connection_info(struct connection *c);
void check_keepalive_connections();
#ifdef DEBUG
void check_queue_bugs();
#endif


static inline int
getpri(struct connection *c)
{
	int i;

	for (i = 0; i < N_PRI; i++)
		if (c->pri[i])
			return i;

	internal("connection has no owner");

	return N_PRI;
}

long
connect_info(int type)
{
	int i = 0;
	struct connection *ce;
	struct k_conn *cee;

	switch (type) {
		case CI_FILES:
			foreach(ce, queue) i++;
			return i;
		case CI_CONNECTING:
			foreach(ce, queue) i += ce->state > S_WAIT && ce->state < S_TRANS;
			return i;
		case CI_TRANSFER:
			foreach(ce, queue) i += ce->state == S_TRANS;
			return i;
		case CI_KEEP:
			foreach(cee, keepalive_connections) i++;
			return i;
		case CI_LIST:
			return (long) &queue;
		default:
			internal("cache_info: bad request");
	}
	return 0;
}

int
connection_disappeared(struct connection *c, tcount count)
{
	struct connection *d;

	foreach(d, queue)
		if (c == d && count == d->count)
			return 0;

	return 1;
}

struct h_conn *
is_host_on_list(struct connection *c)
{
	unsigned char *ho = get_host_name(c->url);
	struct h_conn *h;

	if (!ho) return NULL;
	foreach(h, h_conns) if (!strcmp(h->host, ho)) {
		mem_free(ho);
		return h;
	}
	mem_free(ho);

	return NULL;
}

void
stat_timer(struct connection *c)
{
	ttime a;
	struct remaining_info *r = &c->prg;

	r->loaded = c->received;
	r->size = c->est_length;
	r->pos = c->from;
	if (r->size < r->pos && r->size != -1)
		r->size = c->from;
	r->dis_b += a = get_time() - r->last_time;

	while (r->dis_b >= SPD_DISP_TIME * CURRENT_SPD_SEC) {
		r->cur_loaded -= r->data_in_secs[0];
		memmove(r->data_in_secs, r->data_in_secs + 1,
			sizeof(int) * (CURRENT_SPD_SEC - 1));
		r->data_in_secs[CURRENT_SPD_SEC - 1] = 0;
		r->dis_b -= SPD_DISP_TIME;
	}

	r->data_in_secs[CURRENT_SPD_SEC - 1] += r->loaded - r->last_loaded;
	r->cur_loaded += r->loaded - r->last_loaded;
	r->last_loaded = r->loaded;
	r->last_time += a;
	r->elapsed += a;
	r->timer = install_timer(SPD_DISP_TIME, (void (*)(void *))stat_timer, c);
	if (!st_r) send_connection_info(c);
}

void
setcstate(struct connection *c, int state)
{
	struct status *stat;

	if (c->state < 0 && state >= 0)
		c->prev_error = c->state;

	c->state = state;
	if (c->state == S_TRANS) {
		struct remaining_info *r = &c->prg;

		if (r->timer == -1) {
			tcount count = c->count;
			if (!r->valid) {
				memset(r, 0, sizeof(struct remaining_info));
				r->valid = 1;
			}
			r->last_time = get_time();
			r->last_loaded = r->loaded;
			st_r = 1;
			stat_timer(c);
			st_r = 0;
			if (connection_disappeared(c, count))
				return;
		}

	} else {
		struct remaining_info *r = &c->prg;

		if (r->timer != -1) {
			kill_timer(r->timer);
			r->timer = -1;
		}
	}

	foreach(stat, c->statuss) {
		stat->state = state;
		stat->prev_error = c->prev_error;
	}

	if (state >= 0) send_connection_info(c);
}

struct k_conn *
is_host_on_keepalive_list(struct connection *c)
{
	unsigned char *ho = get_host_and_pass(c->url);
	void (*ph)(struct connection *) = get_protocol_handle(c->url);
	int po = get_port(c->url);
	struct k_conn *h;

	if (po == -1) return NULL;
	if (!ph) return NULL;
	if (!ho) return NULL;

	foreach(h, keepalive_connections)
		if (h->protocol == ph && h->port == po
		    && !strcmp(h->host, ho)) {
			mem_free(ho);
			return h;
		}

	mem_free(ho);
	return NULL;
}

int
get_keepalive_socket(struct connection *c)
{
	struct k_conn *k = is_host_on_keepalive_list(c);

	if (!k) return -1;

	c->sock1 = k->conn;
	c->pf = k->pf;

	del_from_list(k);
	mem_free(k->host);
	mem_free(k);

	return 0;
}

void
abort_all_keepalive_connections()
{
	struct k_conn *k;

	foreach(k, keepalive_connections) {
		mem_free(k->host);
		close(k->conn);
	}
	free_list(keepalive_connections);
	check_keepalive_connections();
}

void
free_connection_data(struct connection *c)
{
	struct h_conn *h;

	if (c->sock1 != -1) set_handlers(c->sock1, NULL, NULL, NULL, NULL);
	if (c->sock2 != -1) set_handlers(c->sock2, NULL, NULL, NULL, NULL);
	close_socket(NULL, &c->sock2);
	if (!c->running) {
		internal("connection already suspended");
	}
	c->running = 0;
	if (c->dnsquery) {
		kill_dns_request(&c->dnsquery);
	}
	if (c->conn_info) {
		if (((struct conn_info *) c->conn_info)->addr)
			mem_free(((struct conn_info *) c->conn_info)->addr);
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
	if (active_connections < 0) {
		internal("active connections underflow");
		active_connections = 0;
	}

	if (c->state != S_WAIT) {
		h = is_host_on_list(c);
		if (h) {
			h->conn--;
			if (!h->conn) {
				del_from_list(h);
				mem_free(h->host);
				mem_free(h);
			}
		} else {
			internal("suspending connection that is not on the list (state %d)", c->state);
		}
	}
}

void
send_connection_info(struct connection *c)
{
	int st = c->state;
	tcount count = c->count;
	struct status *stat = c->statuss.next;

	while ((void *)stat != &c->statuss) {
		stat->ce = c->cache;
		stat = stat->next;
		if (stat->prev->end)
			stat->prev->end(stat->prev, stat->prev->data);
		if (st >= 0 && connection_disappeared(c, count))
			return;
	}
}

void
del_connection(struct connection *c)
{
	del_from_list(c);
	send_connection_info(c);
	mem_free(c->url);
	mem_free(c);
}

void
add_keepalive_socket(struct connection *c, ttime timeout)
{
	struct k_conn *k;

	free_connection_data(c);
	if (c->sock1 == -1) {
		internal("keepalive connection not connected");
		goto del;
	}

	k = mem_alloc(sizeof(struct k_conn));
	if (!k) goto close;

	k->port = get_port(c->url);
	if (k->port == -1) goto free_and_close;

	k->protocol = get_protocol_handle(c->url);
	if (!k->protocol) goto free_and_close;

	k->host = get_host_and_pass(c->url);
	if (!k->host) {

free_and_close:
		mem_free(k);
		del_connection(c);
		goto close;
	}

	k->pf = c->pf;
	k->conn = c->sock1;
	k->timeout = timeout;
	k->add_time = get_time();
	add_to_list(keepalive_connections, k);

del:
	del_connection(c);
#ifdef DEBUG
	check_queue_bugs();
#endif
	register_bottom_half((void (*)(void *))check_queue, NULL);
	return;

close:
	close(c->sock1);
#ifdef DEBUG
	check_queue_bugs();
#endif
	register_bottom_half((void (*)(void *))check_queue, NULL);
}

void
del_keepalive_socket(struct k_conn *kc)
{
	del_from_list(kc);
	close(kc->conn);
	mem_free(kc->host);
	mem_free(kc);
}

void
keepalive_timer(void *x)
{
	keepalive_timeout = -1;
	check_keepalive_connections();
}

void
check_keepalive_connections()
{
	struct k_conn *kc;
	ttime ct = get_time();
	int p = 0;

	if (keepalive_timeout != -1) {
		kill_timer(keepalive_timeout);
		keepalive_timeout = -1;
	}

	foreach(kc, keepalive_connections) {
		if (can_read(kc->conn) || ct - kc->add_time > kc->timeout) {
			kc = kc->prev;
			del_keepalive_socket(kc->next);
		} else {
			p++;
		}
	}

	for (; p > MAX_KEEPALIVE_CONNECTIONS; p--)
		if (!list_empty(keepalive_connections))
			del_keepalive_socket(keepalive_connections.prev);
		else internal("keepalive list empty");

	if (!list_empty(keepalive_connections))
		keepalive_timeout = install_timer(KEEPALIVE_CHECK_TIME,
				                  keepalive_timer, NULL);
}

void
add_to_queue(struct connection *c)
{
	struct connection *cc;
	int pri = getpri(c);

	foreach(cc, queue)
		if (getpri(cc) > pri)
			break;

	add_at_pos(cc->prev, c);
}

void
sort_queue()
{
	struct connection *c, *n;
	int swp;

	do {
		swp = 0;
		foreach(c, queue) {
			if ((void *)c->next != &queue) {
				if (getpri(c->next) < getpri(c)) {
					n = c->next;
					del_from_list(c);
					add_at_pos(n, c);
					swp = 1;
				}
			}
		}
	} while (swp);
}

void
interrupt_connection(struct connection *c)
{
	if (c->ssl == (void *)-1) c->ssl = 0;
	if (c->ssl) {
		free_ssl(c->ssl);
		c->ssl=NULL;
	}

	close_socket(c, &c->sock1);
	free_connection_data(c);
}

void
suspend_connection(struct connection *c)
{
	interrupt_connection(c);
	setcstate(c, S_WAIT);
}

int
try_to_suspend_connection(struct connection *c, unsigned char *ho)
{
	int pri = getpri(c);
	struct connection *d;

	foreachback(d, queue) {
		if (getpri(d) <= pri) return -1;
		if (d->state == S_WAIT) continue;
		if (d->unrestartable == 2 && getpri(d) < PRI_CANCEL) continue;
		if (ho) {
			unsigned char *h = get_host_name(d->url);

			if (!h) continue;
			if (strcmp(h, ho)) {
				mem_free(h);
				continue;
			}
			mem_free(h);
		}
		suspend_connection(d);
		return 0;
	}

	return -1;
}

void
run_connection(struct connection *c)
{
	struct h_conn *hc;
	void (*func)(struct connection *);

	if (c->running) {
		internal("connection already running");
		return;
	}

	func = get_protocol_handle(c->url);
	if (!func) {
		setcstate(c, S_BAD_URL);
		del_connection(c);
		return;
	}

	hc = is_host_on_list(c);
	if (!hc) {
		hc = mem_alloc(sizeof(struct h_conn));
		if (!hc) {
			setcstate(c, S_OUT_OF_MEM);
			del_connection(c);
			return;
		}
		hc->host = get_host_name(c->url);
		if (!hc->host) {
			setcstate(c, S_BAD_URL);
			del_connection(c);
			mem_free(hc);
			return;
		}
		hc->conn = 0;
		add_to_list(h_conns, hc);
	}
	hc->conn++;
	active_connections++;
	c->running = 1;
	func(c);
}

void
retry_connection(struct connection *c)
{
	interrupt_connection(c);
	if (c->unrestartable >= 2
	    || ++c->tries >= get_opt_int("connection.retries")) {
		/*send_connection_info(c);*/
		del_connection(c);
#ifdef DEBUG
		check_queue_bugs();
#endif
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
	del_connection(c);
#ifdef DEBUG
	check_queue_bugs();
#endif
	register_bottom_half((void (*)(void *))check_queue, NULL);
}

/* Set certain state on a connection and then abort the connection. */
void
abort_conn_with_state(struct connection *conn, int state)
{
	setcstate(conn, state);
	abort_connection(conn);
}

/* Set certain state on a connection and then retry the connection. */
void
retry_conn_with_state(struct connection *conn, int state)
{
	setcstate(conn, state);
	retry_connection(conn);
}

int
try_connection(struct connection *c)
{
	struct h_conn *hc = is_host_on_list(c);

	if (hc) {
		if (hc->conn >= get_opt_int("connection.max_connections_to_host"))
			return try_to_suspend_connection(c, hc->host) ? 0 : -1;
	}

	if (active_connections >= get_opt_int("connection.max_connections")) {
		return try_to_suspend_connection(c, NULL) ? 0 : -1;
	}

	run_connection(c);
	return 1;
}


#ifdef DEBUG
void
check_queue_bugs()
{
	struct connection *d;
	int p = 0, ps = 0;
	int cc;

again:
	cc = 0;
	foreach(d, queue) {
		int q = getpri(d);

		cc += d->running;
		if (q < p) {
			if (!ps) {
				internal("queue is not sorted");
				sort_queue();
				ps = 1;
				goto again;
			} else {
				internal("queue is not sorted even after sort_queue!");
				break;
			}
		} else {
			p = q;
		}

		if (d->state < 0) {
			internal("interrupted connection on queue (conn %s, state %d)",
				 d->url, d->state);
			d = d->prev;
			abort_connection(d->next);
		}
	}

	if (cc != active_connections) {
		internal("bad number of active connections (counted %d, stored %d)",
			 cc, active_connections);
		active_connections = cc;
	}
}
#endif

void
check_queue()
{
	struct connection *c;

again:
	c = queue.next;
#ifdef DEBUG
	check_queue_bugs();
#endif
	check_keepalive_connections();

	while (c != (struct connection *)&queue) {
		struct connection *d;
		int cp = getpri(c);

		for (d = c; d != (struct connection *)&queue && getpri(d) == cp;) {
			struct connection *dd = d;

			d = d->next;
			if (!dd->state && is_host_on_keepalive_list(dd)
			    && try_connection(dd)) goto again;
		}

		for (d = c; d != (struct connection *)&queue && getpri(d) == cp;) {
			struct connection *dd = d;

			d = d->next;
			if (!dd->state && try_connection(dd)) goto again;
		}
		c = d;
	}

again2:
	foreachback(c, queue) {
		if (getpri(c) < PRI_CANCEL) break;
		if (c->state == S_WAIT) {
			setcstate(c, S_INTERRUPTED);
			del_connection(c);
			goto again2;
		}
	}

#ifdef DEBUG
	check_queue_bugs();
#endif
}

static unsigned char *
get_proxy_worker(unsigned char *url, unsigned char *proxy)
{
	int l = strlen(url);

	if (proxy) {
		if (!*proxy) proxy = NULL;  /* "" from script_hook_get_proxy() */
	} else {
		if (*get_opt_str("protocol.http.proxy.host") && l >= 7
		    && !strncasecmp(url, "http://", 7))
			proxy = get_opt_str("protocol.http.proxy.host");
		if (*get_opt_str("protocol.ftp.proxy.host") && l >= 6
		    && !strncasecmp(url, "ftp://", 6))
			proxy = get_opt_str("protocol.ftp.proxy.host");
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
	unsigned char *tmp, *ret;

	tmp = script_hook_get_proxy(url);
	ret = get_proxy_worker(url, tmp);
	if (tmp) mem_free(tmp);
	return ret;
#else
	return get_proxy_worker(url, NULL);
#endif
}

int
load_url(unsigned char *url, unsigned char *prev_url,
	 struct status *stat, int pri, enum cache_mode cache_mode)
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
	foreach(c, queue) {
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
		if (stat) {
			stat->ce = e;
			stat->state = S_OK;
			if (stat->end) stat->end(stat, stat->data);
		}
		return 0;
	}

	u = get_proxy(url);
	if (!u) {
		if (stat) stat->end(stat, stat->data);
		return -1;
	}

	foreach(c, queue) {
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

	c = mem_alloc(sizeof(struct connection));
	if (!c) {
		if (stat) stat->end(stat, stat->data);
		mem_free(u);
		return -1;
	}
	memset(c, 0, sizeof(struct connection));

	c->count = connection_count++;
	c->url = u;
	c->prev_url = prev_url;
	c->running = 0;
	c->prev_error = 0;

	if (cache_mode >= NC_RELOAD || !e || e->frag.next == &e->frag
	    || ((struct fragment *) e->frag.next)->offset) {
		c->from = 0;
	} else {
		c->from = ((struct fragment *) e->frag.next)->length;
	}

	memset(c->pri, 0, sizeof c->pri);
	c->pri[pri] = 1;
	c->cache_mode = cache_mode;
	c->sock1 = c->sock2 = -1;
	c->dnsquery = NULL;
	c->conn_info = NULL;
	c->info = NULL;
	c->buffer = NULL;
	c->cache = NULL;
	c->tries = 0;
	init_list(c->statuss);
	c->est_length = -1;
	c->unrestartable = 0;
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


/* FIXME: one object in more connections */
void
change_connection(struct status *oldstat, struct status *newstat,
		  int newpri)
{
	struct connection *c;
	int oldpri;

	if (!oldstat) {
		internal("change_connection: oldstat == NULL");
		return;
	}

	oldpri = oldstat->pri;
	if (oldstat->state < 0) {
		if (newstat) {
			newstat->ce = oldstat->ce;
			newstat->state = oldstat->state;
			newstat->prev_error = oldstat->prev_error;
			if (newstat->end) newstat->end(newstat, newstat->data);
		}
		return;
	}

#ifdef DEBUG
	check_queue_bugs();
#endif

	c = oldstat->c;

	c->pri[oldpri]--;
	if (c->pri[oldpri] < 0) {
		internal("priority counter underflow");
		c->pri[oldpri] = 0;
	}

	c->pri[newpri]++;
	del_from_list(oldstat);
	oldstat->state = S_INTERRUPTED;

	if (newstat) {
		newstat->prg = &c->prg;
		add_to_list(c->statuss, newstat);
		newstat->state = c->state;
		newstat->prev_error = c->prev_error;
		newstat->pri = newpri;
		newstat->c = c;
		newstat->ce = c->cache;
	}

	if (c->detached && !newstat) {
		setcstate(c, S_INTERRUPTED);
		abort_connection(c);
	}

	sort_queue();

#ifdef DEBUG
	check_queue_bugs();
#endif

	register_bottom_half((void (*)(void *))check_queue, NULL);
}

/* This will remove 'pos' bytes from the start of the cache for the specified
 * connection, if the cached object is already too big. */
void
detach_connection(struct status *stat, int pos)
{
	struct connection *conn;

	if (stat->state < 0) return;

	conn = stat->c;
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
		if (!total_pri)
			internal("detaching free connection");

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

void
connection_timeout(struct connection *c)
{
	c->timer = -1;
	setcstate(c, S_TIMEOUT);
	if (c->dnsquery) {
		abort_connection(c);
	} else if (c->conn_info) {
		dns_found(c, 0); /* jump to next addr */
		if (c->conn_info) set_timeout(c);
	} else {
		retry_connection(c);
	}
}

void
connection_timeout_1(struct connection *c)
{
	c->timer = install_timer((c->unrestartable ? get_opt_int("connection.unrestartable_receive_timeout")
						   : get_opt_int("connection.receive_timeout"))
				 * 500, (void (*)(void *)) connection_timeout, c);
}

void
set_timeout(struct connection *c)
{
	if (c->timer != -1) kill_timer(c->timer);
	c->timer = install_timer((c->unrestartable ? get_opt_int("connection.unrestartable_receive_timeout")
						   : get_opt_int("connection.receive_timeout"))
				 * 500, (void (*)(void *))connection_timeout_1, c);
}

void
reset_timeout(struct connection *c)
{
	if (c->timer != -1) {
		kill_timer(c->timer);
		c->timer = -1;
	}
}

void
abort_all_connections()
{
	while(queue.next != &queue) {
		setcstate(queue.next, S_INTERRUPTED);
		abort_connection(queue.next);
	}

	abort_all_keepalive_connections();
}

void
abort_background_connections()
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

		if (getpri(c) >= PRI_CANCEL) {
			setcstate(c, S_INTERRUPTED);
			abort_connection(c);
		} else i++;
	}
}

int
is_entry_used(struct cache_entry *e)
{
	struct connection *c;

	foreach(c, queue)
		if (c->cache == e)
			return 1;

	return 0;
}

