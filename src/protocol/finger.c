/* Internal "finger" protocol implementation */
/* $Id: finger.c,v 1.8 2002/12/07 20:05:57 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/cache.h"
#include "lowlevel/connect.h"
#include "lowlevel/sched.h"
#include "protocol/finger.h"
#include "protocol/url.h"
#include "util/memory.h"
#include "util/string.h"

static void finger_send_request(struct connection *);
static void finger_sent_request(struct connection *);
static void finger_get_response(struct connection *, struct read_buffer *);
static void finger_end_request(struct connection *, int);

void
finger_func(struct connection *c)
{
	int p;

	set_timeout(c);

	p = get_port(c->url);
	if (p == -1) {
		abort_conn_with_state(c, S_INTERNAL);
		return;
	}

	c->from = 0;
	make_connection(c, p, &c->sock1, finger_send_request);
}

static void
finger_send_request(struct connection *c)
{
	unsigned char *req = init_str();
	int rl = 0;
	unsigned char *user = get_user_name(c->url);

	if (!req) return;
	/* add_to_str(&req, &rl, "/W"); */

	if (user) {
		add_to_str(&req, &rl, " ");
		add_to_str(&req, &rl, user);
		mem_free(user);
	}
	add_to_str(&req, &rl, "\r\n");
	write_to_socket(c, c->sock1, req, rl, finger_sent_request);
	mem_free(req);
	setcstate(c, S_SENT);
}

static void
finger_sent_request(struct connection *c)
{
	struct read_buffer *rb;

	set_timeout(c);
	rb = alloc_read_buffer(c);
	if (!rb) return;
	rb->close = 1;
	read_from_socket(c, c->sock1, rb, finger_get_response);
}

static void
finger_get_response(struct connection *c, struct read_buffer *rb)
{
	struct cache_entry *e;
	int l;

	set_timeout(c);

	if (get_cache_entry(c->url, &e)) {
		abort_conn_with_state(c, S_OUT_OF_MEM);
		return;
	}
	c->cache = e;

	if (rb->close == 2) {
		finger_end_request(c, S_OK);
		return;
	}

	l = rb->len;
	c->received += l;

	if (add_fragment(c->cache, c->from, rb->data, l) == 1)
		c->tries = 0;

	c->from += l;
	kill_buffer_data(rb, l);
	read_from_socket(c, c->sock1, rb, finger_get_response);
	setcstate(c, S_TRANS);
}

static void
finger_end_request(struct connection *c, int state)
{
	setcstate(c, state);

	if (c->state == S_OK) {
		if (c->cache) {
			truncate_entry(c->cache, c->from, 1);
			c->cache->incomplete = 0;
		}
	}
	abort_connection(c);
}
