/* $Id: date.h,v 1.1 2002/03/17 21:53:09 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTP_DATE_H
#define EL__PROTOCOL_HTTP_DATE_H

#ifdef HAVE_TIME_H
#include <time.h>
#endif

time_t parse_http_date(const char *);

#endif
