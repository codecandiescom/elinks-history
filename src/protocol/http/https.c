/* Internal "https" protocol implementation */
/* $Id: https.c,v 1.10 2003/06/26 20:04:40 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "protocol/http/http.h"
#include "protocol/http/https.h"
#include "protocol/protocol.h"
#include "sched/sched.h"

static void
https_func(struct connection *c)
{
#ifdef HAVE_SSL
	c->ssl = (void *)-1;
	http_protocol_backend.func(c);
#else
	abort_conn_with_state(c, S_NO_SSL);
#endif
}

struct protocol_backend https_protocol_backend = {
	/* name: */			"https",
	/* port: */			443,
	/* func: */			https_func,
	/* nc_func: */			NULL,
	/* free_syntax: */		0,
	/* need_slashes: */		1,
	/* need_slash_after_host: */	1,
};
