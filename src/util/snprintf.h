/* $Id: snprintf.h,v 1.1 2003/01/20 17:50:33 pasky Exp $ */

#ifndef EL__UTIL_SNPRINTF_H
#define EL__UTIL_SNPRINTF_H

#include <stdarg.h>

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
