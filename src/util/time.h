/* $Id: time.h,v 1.2 2002/09/01 11:57:05 pasky Exp $ */

#ifndef EL__LOWLEVEL_TTIME_H
#define EL__LOWLEVEL_TTIME_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

typedef time_t ttime;

ttime get_time();

#endif
