/* Pseudo about: protocol implementation */
/* $Id: about.c,v 1.9 2005/02/23 21:52:08 jonas Exp $ */

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
	if (cached && !cached->content_type) {
		normalize_cache_entry(cached, 0);

		/* Set content to known type */
		mem_free_set(&cached->content_type, stracpy("text/html"));
	}

	conn->cached = cached;
	abort_conn_with_state(conn, S_OK);
}
