/* $Id: date.h,v 1.5 2005/03/29 01:02:29 jonas Exp $ */

#ifndef EL__PROTOCOL_DATE_H
#define EL__PROTOCOL_DATE_H

#include "util/ttime.h"

/* Expects HH:MM[:SS] or HH:MM[P|A]M, with HH <= 23, MM <= 59, SS <= 59.
 * Updates tm, updates time on success and returns 0 on failure, otherwise 1. */
int parse_time(const unsigned char **time, struct tm *tm, unsigned char *end);

/* Parses the following date formats:
 *
 * Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
 * Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
 * Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
 */
time_T parse_date(const unsigned char *date);

#endif
