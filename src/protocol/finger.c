/* Internal "finger" protocol implementation */
/* $Id: finger.c,v 1.31 2004/04/03 14:13:48 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
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
finger_func(struct connection *conn)
{
	set_connection_timeout(conn);
	conn->from = 0;
	make_connection(conn, get_uri_port(conn->uri), &conn->socket, finger_send_request);
}

static void finger_send_request(struct connection *conn)
{
	struct string req;

	if (!init_string(&req)) return;
	/* add_to_string(&req, &rl, "/W"); */

	if (conn->uri->user) {
		add_char_to_string(&req, ' ');
		add_bytes_to_string(&req, conn->uri->user, conn->uri->userlen);
	}
	add_to_string(&req, "\r\n");
	write_to_socket(conn, conn->socket, req.source, req.length, finger_sent_request);
	done_string(&req);
	set_connection_state(conn, S_SENT);
}

static void
finger_sent_request(struct connection *conn)
{
	struct read_buffer *rb;

	set_connection_timeout(conn);
	rb = alloc_read_buffer(conn);
	if (!rb) return;
	rb->close = 1;
	read_from_socket(conn, conn->socket, rb, finger_get_response);
}

static void
finger_get_response(struct connection *conn, struct read_buffer *rb)
{
	struct cache_entry *cached = get_cache_entry(conn->uri);
	int l;

	set_connection_timeout(conn);

	if (!cached) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}
	conn->cache = cached;

	if (rb->close == 2) {
		finger_end_request(conn, S_OK);
		return;
	}

	l = rb->len;
	conn->received += l;

	if (add_fragment(conn->cache, conn->from, rb->data, l) == 1)
		conn->tries = 0;

	conn->from += l;
	kill_buffer_data(rb, l);
	read_from_socket(conn, conn->socket, rb, finger_get_response);
	set_connection_state(conn, S_TRANS);
}

static void
finger_end_request(struct connection *conn, enum connection_state state)
{
	set_connection_state(conn, state);

	if (conn->state == S_OK) {
		if (conn->cache) {
			truncate_entry(conn->cache, conn->from, 1);
			conn->cache->incomplete = 0;
		}
	}
	abort_connection(conn);
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
