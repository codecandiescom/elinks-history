/* $Id: http.h,v 1.4 2003/01/01 20:30:35 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTP_H
#define EL__PROTOCOL_HTTP_H

#include "sched/sched.h"

void http_func(struct connection *);
void proxy_func(struct connection *);

#endif
