/* $Id: snprintf.h,v 1.5 2003/06/07 09:59:44 pasky Exp $ */

#ifndef EL__UTIL_SNPRINTF_H
#define EL__UTIL_SNPRINTF_H

#include <stdarg.h>

/* XXX: This is not quite the best place for it, perhaps. But do we have
 * a better one now? --pasky */
#ifndef VA_COPY
#ifdef HAVE_VA_COPY
#define VA_COPY(dest, src) __va_copy(dest, src)
#else
#define VA_COPY(dest, src) (dest) = (src)
#endif
#endif

#include <stdio.h> /* The system's snprintf(). */

#if !defined(HAVE_VSNPRINTF) || !defined(HAVE_C99_VSNPRINTF)
int vsnprintf(char *str, size_t count, const char *fmt, va_list args);
#endif

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_C99_SNPRINTF)
int snprintf(char *str, size_t count, const char *fmt, ...);
#endif

#ifndef HAVE_VASPRINTF
int vasprintf(char **ptr, const char *format, va_list ap);
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **ptr, const char *format, ...);
#endif

#endif
