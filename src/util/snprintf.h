/* $Id: snprintf.h,v 1.3 2003/06/07 08:30:20 zas Exp $ */

#ifndef EL__UTIL_SNPRINTF_H
#define EL__UTIL_SNPRINTF_H

/* --enable-own-libc will force to use internal *snprintf() stuff */
#if defined(USE_OWN_LIBC) || \
    !(defined(HAVE_SNPRINTF) && defined(HAVE_VSNPRINTF) \
      && defined(HAVE_C99_VSNPRINTF))
#define USE_OUR_SNPRINTF_C
#endif

#include <stdarg.h>

/* only include stdio.h if we are not re-defining snprintf or vsnprintf */
#if !defined(USE_OUR_SNPRINTF_C)
#include <stdio.h> /* The system's snprintf(). */
#endif


/* Check for VA_COPY macro. */
#ifndef VA_COPY
/* Test __va_copy() or va_copy() or both ?? */
#ifdef HAVE_VA_COPY /* __va_copy() */
#define VA_COPY(dest, src) __va_copy(dest, src)
#else
/* This one should be portable. */
#define VA_COPY(dest, src) memcpy(&(dest), (src), sizeof(va_list))
#endif
#endif

#if defined(USE_OUR_SNPRINTF_C) || \
    (!defined(HAVE_VSNPRINTF) || !defined(HAVE_C99_VSNPRINTF))
#undef vsnprintf
int vsnprintf(char *str, size_t count, const char *fmt, va_list args);
#define USE_OUR_VSNPRINTF
#endif

/* yes this really must be a ||. Don't muck wiith this (tridge)
 *
 * The logic for these two is that we need our own definition if the
 * OS *either* has no definition of *sprintf, or if it does have one
 * that doesn't work properly according to the autoconf test.  Perhaps
 * these should really be smb_snprintf to avoid conflicts with buggy
 * linkers? -- mbp
 */
#if defined(USE_OUR_SNPRINTF_C) || \
    (!defined(HAVE_SNPRINTF) || !defined(HAVE_C99_SNPRINTF))
#undef snprintf
int snprintf(char *str, size_t count, const char *fmt, ...);
#define USE_OUR_SNPRINTF
#endif

#if defined(USE_OUR_SNPRINTF_C) || !defined(HAVE_VASPRINTF)
#undef vasprintf
int vasprintf(char **ptr, const char *format, va_list ap);
#define USE_OUR_VASPRINTF
#endif

#if defined(USE_OUR_SNPRINTF_C) || !defined(HAVE_ASPRINTF)
#undef asprintf
int asprintf(char **ptr, const char *format, ...);
#define USE_OUR_ASPRINTF
#endif

#endif
