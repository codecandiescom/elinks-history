/* $Id: date.h,v 1.2 2002/09/01 11:57:05 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTP_DATE_H
#define EL__PROTOCOL_HTTP_DATE_H

#include "lowlevel/ttime.h"

ttime parse_http_date(const char *);

#endif
