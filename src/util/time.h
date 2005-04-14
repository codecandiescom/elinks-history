/* $Id: time.h,v 1.16 2005/04/14 10:32:22 zas Exp $ */

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

/* Redefine a timeval that has all fields signed so calculations
 * will be simplified on rare systems that define timeval with
 * unsigned fields. */
typedef struct { long int sec; long int usec; } timeval_T;

#define tv2tT(tv, tT) do { \
	(tT)->sec = (long int) ((tv)->tv_sec); \
	(tT)->usec = (long int) ((tv)->tv_usec); \
} while (0)

#define tT2tv(tT, tv) do { \
	(tv)->tv_sec = (tT)->sec; \
	(tv)->tv_usec = (tT)->usec; \
} while (0)

void milliseconds_to_timeval(timeval_T *t, long int milliseconds);
int timeval_is_positive(struct timeval *tv);
void limit_timeval_to_zero(struct timeval *tv);

#endif
