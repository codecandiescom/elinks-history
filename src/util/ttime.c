/* Time operations */
/* $Id: ttime.c,v 1.8 2005/02/05 22:36:37 jonas Exp $ */

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


ttime
get_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (ttime) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
