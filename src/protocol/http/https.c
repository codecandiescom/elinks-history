/* Internal "https" protocol implementation */
/* $Id: https.c,v 1.20 2004/04/29 23:22:14 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "protocol/http/http.h"
#include "protocol/http/https.h"
#include "protocol/protocol.h"
#include "sched/connection.h"
#include "ssl/ssl.h"

static void
https_func(struct connection *conn)
{
#ifdef CONFIG_SSL
	if (init_ssl_connection(conn) == S_SSL_ERROR)
		abort_conn_with_state(conn, S_SSL_ERROR);
	else
		http_protocol_backend.handler(conn);
#else
	abort_conn_with_state(conn, S_NO_SSL);
#endif
}

struct protocol_backend https_protocol_backend = {
	/* name: */			"https",
	/* port: */			443,
	/* handler: */			https_func,
	/* external_handler: */		NULL,
	/* free_syntax: */		0,
	/* need_slashes: */		1,
	/* need_slash_after_host: */	1,
};
