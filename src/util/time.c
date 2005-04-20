/* Time operations */
/* $Id: time.c,v 1.35 2005/04/20 10:12:01 zas Exp $ */

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
timeval_T *
get_timeval(timeval_T *t)
{
#ifdef HAVE_GETTIMEOFDAY
	struct timeval tv;

	gettimeofday(&tv, NULL);
	tv2tT(&tv, t);
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

	return t;
}

/* Subtract an interval to a timeval, it ensures that
 * result is never negative. */
timeval_T *
timeval_sub_interval(timeval_T *t, timeval_T *interval)
{
	t->sec  -= interval->sec;
	if (t->sec < 0) {
		t->sec = 0;
		t->usec = 0;
		return t;
	}

	t->usec -= interval->usec;

	while (t->usec < 0) {
		t->usec += 1000000;
		t->sec--;
	}

	if (t->sec < 0) {
		t->sec = 0;
		t->usec = 0;
	}

	return t;
}

timeval_T *
timeval_sub(timeval_T *res, timeval_T *older, timeval_T *newer)
{
	res->sec  = newer->sec - older->sec;
	res->usec = newer->usec - older->usec;

	while (res->usec < 0) {
		res->usec += 1000000;
		res->sec--;
	}

	return res;
}

timeval_T *
timeval_add(timeval_T *res, timeval_T *base, timeval_T *t)
{
	res->sec  = base->sec + t->sec;
	res->usec = base->usec + t->usec;

	while (res->usec >= 1000000) {
		res->usec -= 1000000;
		res->sec++;
	}

	return res;
}

timeval_T *
timeval_add_interval(timeval_T *t, timeval_T *interval)
{
	t->sec  += interval->sec;
	t->usec += interval->usec;

	while (t->usec >= 1000000) {
		t->usec -= 1000000;
		t->sec++;
	}

	return t;
}

timeval_T *
double_to_timeval(timeval_T *t, double x)
{
	t->sec  = (long int) x;
	t->usec = (long int) ((x - (double) t->sec) * 1000000);

	return t;
}

timeval_T *
milliseconds_to_timeval(timeval_T *t, long int milliseconds)
{
	t->sec = milliseconds / 1000;
	t->usec = (milliseconds % 1000) * 1000;

	return t;
}

timeval_T *
seconds_to_timeval(timeval_T *t, long int seconds)
{
	t->sec = seconds;
	t->usec = 0;

	return t;
}

long int
timeval_to_milliseconds(timeval_T *t)
{
	return t->sec * 1000L + t->usec / 1000L;
}

long int
timeval_to_seconds(timeval_T *t)
{
	return t->sec + t->usec / 1000000L;
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

	return t1->usec - t2->usec;
}

