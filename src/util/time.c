/* Time operations */
/* $Id: time.c,v 1.25 2005/04/15 14:14:48 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "elinks.h"

#include "osdep/win32/win32.h" /* For gettimeofday stub */
#include "util/time.h"


/* Get the current time.
 * It attempts to use available functions, granularity
 * may be as worse as 1 second if time() is used. */
void
get_timeval(timeval_T *t)
{
#ifdef HAVE_GETTIMEOFDAY
	struct timeval tv;

	gettimeofday(&tv, NULL);
	tv2tT(tv, t);
#else
#ifdef HAVE_CLOCK_GETTIME
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	t->sec = ts.tv_sec;
	t->usec = ts.tv_nsec / 1000;
#else
	t->sec = time(NULL);
	t->usec = 0;
#endif
#endif
}

double
timeval_diff(timeval_T *older, timeval_T *newer)
{
	timeval_T d;
 
	d.sec = newer->sec - older->sec;
	d.usec = newer->usec - older->usec;
 
	return (double) d.sec + ((double) d.usec / 1000000.0);
}

void
timeval_sub(timeval_T *res, timeval_T *older, timeval_T *newer)
{
	res->sec  = newer->sec - older->sec;
	res->usec = newer->usec - older->usec;

	while (res->usec < 0) {
		res->usec += 1000000;
		res->sec--;
	}
}
 
void
timeval_add(timeval_T *res, timeval_T *base, timeval_T *t)
{
	res->sec  = base->sec + t->sec;
	res->usec = base->usec + t->usec;

	while (res->usec >= 1000000) {
		res->usec -= 1000000;
		res->sec++;
	}
}

void
double_to_timeval(double x, timeval_T *t)
{
	t->sec  = (long int) x;
	t->usec = (long int) ((x - (double) t->sec) * 1000000);
}

void
milliseconds_to_timeval(timeval_T *t, long int milliseconds)
{
	t->sec = milliseconds / 1000;
	t->usec = (milliseconds % 1000) * 1000;
}

long int
timeval_to_milliseconds(timeval_T *t)
{
	return t->sec * 1000L + t->usec / 1000L;
}

int
timeval_is_positive(timeval_T *t)
{
	return (t->sec > 0 || (t->sec == 0 && t->usec > 0));
}

/* Be sure timeval is not negative. */
void
limit_timeval_to_zero(timeval_T *t)
{
	if (t->sec < 0) t->sec = 0;
	if (t->usec < 0) t->usec = 0;
}

/* Returns 1 if t1 > t2
 * -1 if t1 < t2
 * 0 if t1 == t2 */
int
timeval_cmp(timeval_T *t1, timeval_T *t2)
{
	if (t1->sec > t2->sec) return 1;
	if (t1->sec < t2->sec) return -1;
	if (t1->usec > t2->usec) return 1;
	if (t1->usec < t2->usec) return -1;
	return 0;
}

