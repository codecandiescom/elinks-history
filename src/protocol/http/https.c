/* Internal "https" protocol implementation */
/* $Id: https.c,v 1.14 2003/07/05 03:20:31 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "protocol/http/http.h"
#include "protocol/http/https.h"
#include "protocol/protocol.h"
#include "sched/connection.h"

static void
https_func(struct connection *c)
{
#ifdef HAVE_SSL
	c->ssl = get_ssl();
	if (!c->ssl)
		abort_conn_with_state(c, S_SSL_ERROR);
	else
		http_protocol_backend.handler(c);
#else
	abort_conn_with_state(c, S_NO_SSL);
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
