/* Internal "https" protocol implementation */
/* $Id: https.c,v 1.7 2002/09/09 15:55:04 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "links.h"

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
