/* Protocol implementation manager. */
/* $Id: about.c,v 1.1 2004/06/27 21:36:20 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "protocol/protocol.h"
#include "sched/connection.h"
#include "util/memory.h"


void
about_protocol_handler(struct connection *conn)
{
	conn->cached = get_cache_entry(conn->uri);

	/* Only do this the first time */
	if (conn->cached && !conn->cached->head) {
		struct cache_entry *cached = conn->cached;

		cached->incomplete = 0;

		/* Set content to known type */
		cached->head = stracpy("\r\nContent-Type: text/html\r\n");
	}

	abort_conn_with_state(conn, S_OK);
}
