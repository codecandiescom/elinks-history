/* Pseudo about: protocol implementation */
/* $Id: about.c,v 1.10 2005/04/14 00:40:55 jonas Exp $ */

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
	abort_connection(conn, S_OK);
}
