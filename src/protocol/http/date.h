/* $Id: date.h,v 1.3 2003/09/21 12:02:52 zas Exp $ */

#ifndef EL__PROTOCOL_HTTP_DATE_H
#define EL__PROTOCOL_HTTP_DATE_H

#include "lowlevel/ttime.h"

ttime parse_http_date(const unsigned char *);

#endif
