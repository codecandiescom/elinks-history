/* $Id: types.h,v 1.13 2005/06/14 18:02:25 jonas Exp $ */

#ifndef EL__OSDEP_TYPES_H
#define EL__OSDEP_TYPES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifndef SHRT_MAX
#define SHRT_MAX 0x7fff
#endif

#ifndef USHRT_MAX
#define USHRT_MAX 0xffff
#endif

#ifndef INT_MAX
#ifdef MAXINT
#define INT_MAX MAXINT
#else
#define INT_MAX 0x7fffffff
#endif
#endif

#ifndef UINT_MAX
#ifdef MAXUINT
#define UINT_MAX MAXUINT
#else
#define UINT_MAX 0xffffffff
#endif
#endif

#ifndef LONG_MAX
#ifdef MAXLONG
#define LONG_MAX MAXLONG
#else
#define LONG_MAX 0x7fffffff
#endif
#endif

#ifndef HAVE_UINT16_T
#if SIZEOF_CHAR == 2
typedef unsigned char uint16_t;
#elif SIZEOF_SHORT == 2
typedef unsigned short uint16_t;
#elif SIZEOF_INT == 2
typedef unsigned int uint16_t;
#elif SIZEOF_LONG == 2
typedef unsigned long uint16_t;
#elif defined(HAVE_LONG_LONG) && SIZEOF_LONG_LONG == 2
typedef unsigned long long uint16_t;
#else
#error You have no 16-bit integer type. Get in touch with reality.
#endif
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

#ifdef HAVE_LONG_LONG
#define longlong long long
#else
#define longlong long
#endif

#endif
