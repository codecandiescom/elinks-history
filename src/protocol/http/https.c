/* Internal "https" protocol implementation */
/* $Id: https.c,v 1.8 2002/12/07 20:05:57 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "lowlevel/sched.h"
#include "protocol/http/http.h"
#include "protocol/http/https.h"

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
