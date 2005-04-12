/* Internal "finger" protocol implementation */
/* $Id: finger.c,v 1.12 2005/04/12 20:27:37 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/connect.h"
#include "modules/module.h"
#include "protocol/finger/finger.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "util/memory.h"
#include "util/string.h"

struct module finger_protocol_module = struct_module(
	/* name: */		N_("Finger"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

static void
finger_end_request(struct connection *conn, enum connection_state state)
{
	set_connection_state(conn, state);

	if (conn->state == S_OK) {
		if (conn->cached) {
			normalize_cache_entry(conn->cached, conn->from);
		}
	}
	abort_connection(conn);
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
	conn->cached = cached;

	if (rb->state == SOCKET_CLOSED) {
		finger_end_request(conn, S_OK);
		return;
	}

	l = rb->len;
	conn->received += l;

	if (add_fragment(conn->cached, conn->from, rb->data, l) == 1)
		conn->tries = 0;

	conn->from += l;
	kill_buffer_data(rb, l);
	read_from_socket(conn->socket, rb, finger_get_response);
	set_connection_state(conn, S_TRANS);
}

static void
finger_sent_request(struct connection *conn)
{
	struct read_buffer *rb;

	set_connection_timeout(conn);
	rb = alloc_read_buffer(conn->socket);
	if (!rb) return;
	rb->state = SOCKET_END_ONCLOSE;
	read_from_socket(conn->socket, rb, finger_get_response);
}

static void
finger_send_request(struct connection *conn)
{
	struct string req;

	if (!init_string(&req)) return;
	/* add_to_string(&req, &rl, "/W"); */

	if (conn->uri->user) {
		add_char_to_string(&req, ' ');
		add_bytes_to_string(&req, conn->uri->user, conn->uri->userlen);
	}
	add_crlf_to_string(&req);
	write_to_socket(conn->socket, req.source, req.length, S_SENT, finger_sent_request);
	done_string(&req);
}

void
finger_protocol_handler(struct connection *conn)
{
	conn->from = 0;
	make_connection(conn, conn->socket, finger_send_request);
}
