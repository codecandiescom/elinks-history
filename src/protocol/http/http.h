/* $Id: http.h,v 1.2 2002/03/17 18:14:08 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTP_H
#define EL__PROTOCOL_HTTP_H

#include <lowlevel/sched.h>

void http_func(struct connection *);
void proxy_func(struct connection *);

#endif
