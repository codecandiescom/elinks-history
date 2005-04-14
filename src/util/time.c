/* Time operations */
/* $Id: time.c,v 1.15 2005/04/14 10:32:22 zas Exp $ */

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


void
milliseconds_to_timeval(timeval_T *t, long int milliseconds)
{
	t->sec = milliseconds / 1000;
	t->usec = (milliseconds % 1000) * 1000;
}

int
timeval_is_positive(struct timeval *tv)
{
	return (tv->tv_sec > 0 || (tv->tv_sec == 0 && tv->tv_usec > 0));
}

/* Be sure timeval is not negative. */
void
limit_timeval_to_zero(struct timeval *tv)
{
	if (tv->tv_sec < 0) tv->tv_sec = 0;
	if (tv->tv_usec < 0) tv->tv_usec = 0;
}
