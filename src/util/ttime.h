/* $Id: ttime.h,v 1.7 2004/09/24 20:34:11 pasky Exp $ */

#ifndef EL__UTIL_TTIME_H
#define EL__UTIL_TTIME_H

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

/* WARNING: may cause overflows since ttime holds values 1000 times
 * bigger than usual time_t */
typedef time_t ttime;

ttime get_time(void);

#endif
