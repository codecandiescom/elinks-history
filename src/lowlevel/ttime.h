/* $Id: ttime.h,v 1.5 2003/09/26 18:28:47 pasky Exp $ */

#ifndef EL__LOWLEVEL_TTIME_H
#define EL__LOWLEVEL_TTIME_H

#ifdef TIME_WITH_SYS_TIME
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#else
#if defined(TM_IN_SYS_TIME) && defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#elif defined(HAVE_TIME_H)
#include <time.h>
#endif
#endif

/* WARNING: may cause overflows since ttime holds values 1000 times
 * bigger than usual time_t */
typedef time_t ttime;

ttime get_time(void);

#endif
