/* $Id: types.h,v 1.1 2002/08/27 02:13:57 pasky Exp $ */

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
#if SIZEOF_CHAR == 32
typedef char int32_t;
#elif SIZEOF_SHORT == 32
typedef short int32_t;
#elif SIZEOF_INT == 32
typedef int int32_t;
#elif SIZEOF_LONG == 32
typedef long int32_t;
#elif defined(HAVE_LONG_LONG) && SIZEOF_LONG_LONG == 32
typedef long long int32_t;
#else
#error You have no 32-bit integer type. Get in touch with reality.
#endif
#endif

#ifndef HAVE_UINT32_T
#if SIZEOF_CHAR == 32
typedef unsigned char uint32_t;
#elif SIZEOF_SHORT == 32
typedef unsigned short uint32_t;
#elif SIZEOF_INT == 32
typedef unsigned int uint32_t;
#elif SIZEOF_LONG == 32
typedef unsigned long uint32_t;
#elif defined(HAVE_LONG_LONG) && SIZEOF_LONG_LONG == 32
typedef unsigned long long uint32_t;
#else
#error You have no 32-bit integer type. Get in touch with reality.
#endif
#endif

#endif
