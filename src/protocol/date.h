/* $Id: date.h,v 1.2 2004/05/22 19:20:26 jonas Exp $ */

#ifndef EL__PROTOCOL_DATE_H
#define EL__PROTOCOL_DATE_H

#include "util/ttime.h"

ttime parse_http_date(const unsigned char *);

#endif
