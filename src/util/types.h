/* $Id: types.h,v 1.5 2003/07/03 10:59:27 jonas Exp $ */

#ifndef EL__UTIL_TYPES_H
#define EL__UTIL_TYPES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifndef HAVE_INT32_T
#if SIZEOF_CHAR == 4
typedef char int32_t;
#elif SIZEOF_SHORT == 4
typedef short int32_t;
#elif SIZEOF_INT == 4
typedef int int32_t;
#elif SIZEOF_LONG == 4
typedef long int32_t;
#elif defined(HAVE_LONG_LONG) && SIZEOF_LONG_LONG == 4
typedef long long int32_t;
#else
#error You have no 32-bit integer type. Get in touch with reality.
#endif
#endif

#ifndef HAVE_UINT32_T
#if SIZEOF_CHAR == 4
typedef unsigned char uint32_t;
#elif SIZEOF_SHORT == 4
typedef unsigned short uint32_t;
#elif SIZEOF_INT == 4
typedef unsigned int uint32_t;
#elif SIZEOF_LONG == 4
typedef unsigned long uint32_t;
#elif defined(HAVE_LONG_LONG) && SIZEOF_LONG_LONG == 4
typedef unsigned long long uint32_t;
#else
#error You have no 32-bit integer type. Get in touch with reality.
#endif
#endif

/* Historic relict */
typedef unsigned tcount;

#ifdef HAVE_LONG_LONG
#define longlong long long
#else
#define longlong long
#endif

/* *_info() types */
enum info_type {
	INFO_BYTES,
	INFO_FILES,
	INFO_LOCKED,
	INFO_LOADING,
	INFO_TIMERS,
	INFO_TRANSFER,
	INFO_CONNECTING,
	INFO_KEEP,
	INFO_LIST,
};

#endif
