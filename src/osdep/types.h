/* $Id: types.h,v 1.2 2002/09/01 11:37:48 pasky Exp $ */

#ifndef EL__UTIL_TYPES_H
#define EL__UTIL_TYPES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#endif
