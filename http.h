/* $Id: http.h,v 1.3 2002/03/16 22:03:09 pasky Exp $ */

#ifndef EL__HTTP_H
#define EL__HTTP_H

#include "sched.h"

unsigned char *parse_http_header(unsigned char *, unsigned char *, unsigned char **);
unsigned char *parse_header_param(unsigned char *, unsigned char *);
void http_func(struct connection *);
void proxy_func(struct connection *);

#endif
