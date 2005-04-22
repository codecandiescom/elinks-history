/* $Id: time.h,v 1.38 2005/04/22 01:07:12 zas Exp $ */

#ifndef EL__UTIL_TIME_H
#define EL__UTIL_TIME_H

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

typedef time_t time_T;

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

timeval_T *timeval_from_milliseconds(timeval_T *t, long int milliseconds);
timeval_T *timeval_from_seconds(timeval_T *t, long int seconds);
timeval_T *double_to_timeval(timeval_T *t, double x);

long int timeval_to_milliseconds(timeval_T *t);
long int timeval_to_seconds(timeval_T *t);

int timeval_is_positive(timeval_T *t);
void limit_timeval_to_zero(timeval_T *t);
timeval_T *get_timeval(timeval_T *t);
timeval_T *timeval_sub(timeval_T *res, timeval_T *older, timeval_T *newer);
timeval_T *timeval_add(timeval_T *res, timeval_T *base, timeval_T *t);
int timeval_cmp(timeval_T *t1, timeval_T *t2);
timeval_T *timeval_sub_interval(timeval_T *t, timeval_T *interval);
timeval_T *timeval_add_interval(timeval_T *t, timeval_T *interval);

#define timeval_copy(dst, src) copy_struct(dst, src)

#endif
