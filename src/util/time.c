/* Time operations */
/* $Id: time.c,v 1.9 2005/03/05 22:14:32 zas Exp $ */

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
