/* $Id: http.h,v 1.6 2003/11/30 06:42:48 witekfl Exp $ */

#ifndef EL__PROTOCOL_HTTP_HTTP_H
#define EL__PROTOCOL_HTTP_HTTP_H

#include "lowlevel/connect.h"
#include "protocol/protocol.h"
#include "sched/connection.h"
#include "util/blacklist.h"

struct http_version {
	int major;
	int minor;
};

struct http_connection_info {
	enum blacklist_flags bl_flags;
	struct http_version recv_version;
	struct http_version sent_version;

	int close;

#define LEN_CHUNKED -2 /* == we get data in unknown number of chunks */
#define LEN_FINISHED 0
	int length;

	/* Either bytes coming in this chunk yet or "parser state". */
#define CHUNK_DATA_END	-3
#define CHUNK_ZERO_SIZE	-2
#define CHUNK_SIZE	-1
	int chunk_remaining;

	int http_code;
};

extern struct protocol_backend http_protocol_backend;
extern struct protocol_backend proxy_protocol_backend;

void http_got_header(struct connection *, struct read_buffer *);

#endif
