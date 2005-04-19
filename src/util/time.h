/* $Id: time.h,v 1.31 2005/04/19 22:00:34 zas Exp $ */

#ifndef EL__UTIL_TIME_H
#define EL__UTIL_TIME_H

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

/* WARNING: may cause overflows since time_T holds values 1000 times
 * bigger than usual time_t */
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

void milliseconds_to_timeval(timeval_T *t, long int milliseconds);
void seconds_to_timeval(timeval_T *t, long int seconds);
void double_to_timeval(timeval_T *t, double x);

long int timeval_to_milliseconds(timeval_T *t);
int timeval_is_positive(timeval_T *t);
void limit_timeval_to_zero(timeval_T *t);
void get_timeval(timeval_T *t);
void timeval_sub(timeval_T *res, timeval_T *older, timeval_T *newer);
void timeval_add(timeval_T *res, timeval_T *base, timeval_T *t);
int timeval_cmp(timeval_T *t1, timeval_T *t2);
void timeval_sub_interval(timeval_T *t, timeval_T *interval);
void timeval_add_interval(timeval_T *t, timeval_T *interval);

#endif
