/* Internal "finger" protocol implementation */
/* $Id: finger.c,v 1.18 2003/07/14 19:51:32 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/cache.h"
#include "lowlevel/connect.h"
#include "protocol/finger.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "util/memory.h"
#include "util/string.h"

static void finger_send_request(struct connection *);
static void finger_sent_request(struct connection *);
static void finger_get_response(struct connection *, struct read_buffer *);
static void finger_end_request(struct connection *, int);

static void
finger_func(struct connection *c)
{
	set_connection_timeout(c);
	c->from = 0;
	make_connection(c, get_uri_port(&c->uri), &c->sock1, finger_send_request);
}

static void finger_send_request(struct connection *c)
{
	unsigned char *req = init_str();
	int rl = 0;

	if (!req) return;
	/* add_to_str(&req, &rl, "/W"); */

	if (c->uri.user) {
		add_chr_to_str(&req, &rl, ' ');
		add_bytes_to_str(&req, &rl, c->uri.user, c->uri.userlen);
	}
	add_to_str(&req, &rl, "\r\n");
	write_to_socket(c, c->sock1, req, rl, finger_sent_request);
	mem_free(req);
	set_connection_state(c, S_SENT);
}

static void
finger_sent_request(struct connection *c)
{
	struct read_buffer *rb;

	set_connection_timeout(c);
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

	set_connection_timeout(c);

	if (get_cache_entry(c->uri.protocol, &e)) {
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
	set_connection_state(c, S_TRANS);
}

static void
finger_end_request(struct connection *c, enum connection_state state)
{
	set_connection_state(c, state);

	if (c->state == S_OK) {
		if (c->cache) {
			truncate_entry(c->cache, c->from, 1);
			c->cache->incomplete = 0;
		}
	}
	abort_connection(c);
}

struct protocol_backend finger_protocol_backend = {
	/* name: */			"finger",
	/* port: */			79,
	/* handler: */			finger_func,
	/* external_handler: */		NULL,
	/* free_syntax: */		0,
	/* need_slashes: */		1,
	/* need_slash_after_host: */	1,
};
