/* $Id: http.h,v 1.3 2002/05/08 13:55:06 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTP_H
#define EL__PROTOCOL_HTTP_H

#include "lowlevel/sched.h"

void http_func(struct connection *);
void proxy_func(struct connection *);

#endif
