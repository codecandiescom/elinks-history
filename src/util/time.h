/* $Id: time.h,v 1.9 2005/03/05 22:14:32 zas Exp $ */

#ifndef EL__UTIL_TTIME_H
#define EL__UTIL_TTIME_H

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

/* WARNING: may cause overflows since time_T holds values 1000 times
 * bigger than usual time_t */
typedef time_t time_T;

time_T get_time(void);

/* Is using atol() in this way acceptable? It seems
 * non-portable to me; time_T might not be a long. -- Miciah */
#define str_to_time_T(s) ((time_T) atol(s))

#endif
