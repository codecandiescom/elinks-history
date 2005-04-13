/* Time operations */
/* $Id: ttime.c,v 1.10 2005/04/13 16:08:11 zas Exp $ */

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
#include "util/ttime.h"


time_T
get_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (time_T) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}


/* Subtract the `struct timeval' values X and Y, storing the result in RESULT.
 * Return 1 if the difference is negative, otherwise 0.
 * It works even on some peculiar operating systems where the tv_sec member
 * has an unsigned type.
 * Borrowed from GNU glibc documentation. */
int
timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;

		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000;

		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait. tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

void
milliseconds_to_timeval(struct timeval *a, long int milliseconds)
{
	a->tv_sec = milliseconds / 1000;
	a->tv_usec = (milliseconds % 1000) * 1000;
}
