/* Internal "https" protocol implementation */
/* $Id: https.c,v 1.9 2003/01/01 20:30:35 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "protocol/http/http.h"
#include "protocol/http/https.h"
#include "sched/sched.h"

void
https_func(struct connection *c)
{
#ifdef HAVE_SSL
	c->ssl = (void *)-1;
	http_func(c);
#else
	abort_conn_with_state(c, S_NO_SSL);
#endif
}
