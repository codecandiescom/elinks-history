/* Internal "https" protocol implementation */
/* $Id: https.c,v 1.3 2002/03/17 23:16:51 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <links.h>

#include <lowlevel/sched.h>
#include <protocol/http/http.h>
#include <protocol/http/https.h>

void https_func(struct connection *c)
{
#ifdef HAVE_SSL
	c->ssl = (void *)-1;
	http_func(c);
#else
	setcstate(c, S_NO_SSL);
	abort_connection(c);
#endif
}
