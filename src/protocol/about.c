/* Pseudo about: protocol implementation */
/* $Id: about.c,v 1.7 2004/08/13 20:32:15 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "protocol/about.h"
#include "protocol/protocol.h"
#include "sched/connection.h"
#include "util/string.h"


void
about_protocol_handler(struct connection *conn)
{
	struct cache_entry *cached = get_cache_entry(conn->uri);

	/* Only do this the first time */
	if (cached && !cached->head) {
		cached->incomplete = 0;

		/* Set content to known type */
		cached->head = stracpy("\r\nContent-Type: text/html\r\n");
	}

	conn->cached = cached;
	abort_conn_with_state(conn, S_OK);
}
