/* $Id: date.h,v 1.5 2004/05/21 11:39:54 jonas Exp $ */

#ifndef EL__PROTOCOL_HTTP_DATE_H
#define EL__PROTOCOL_HTTP_DATE_H

#include "util/ttime.h"

ttime parse_http_date(const unsigned char *);

#endif
