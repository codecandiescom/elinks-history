/* $Id: date.h,v 1.4 2003/12/01 14:21:49 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTP_DATE_H
#define EL__PROTOCOL_HTTP_DATE_H

#include "util/ttime.h"

ttime parse_http_date(const unsigned char *);

#endif
