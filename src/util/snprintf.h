/* $Id: snprintf.h,v 1.8 2003/06/17 01:16:25 jonas Exp $ */

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


#ifdef USE_OWN_LIBC
#undef HAVE_VSNPRINTF
#undef HAVE_C99_VSNPRINTF
#undef HAVE_SNPRINTF
#undef HAVE_C99_SNPRINTF
#undef HAVE_VASPRINTF
#undef HAVE_ASPRINTF
#else
#include <stdio.h> /* The system's snprintf(). */
#endif


#if !defined(HAVE_VSNPRINTF) || !defined(HAVE_C99_VSNPRINTF)
#undef vsnprintf
#define vsnprintf elinks_vsnprintf
int elinks_vsnprintf(char *str, size_t count, const char *fmt, va_list args);
#endif

#if !defined(HAVE_SNPRINTF)
#undef snprintf
#define snprintf elinks_snprintf
int elinks_snprintf(char *str, size_t count, const char *fmt, ...);
#endif

#ifndef HAVE_VASPRINTF
#undef vasprintf
#define vasprintf elinks_vasprintf
int elinks_vasprintf(char **ptr, const char *format, va_list ap);
#endif

#ifndef HAVE_ASPRINTF
#undef asprintf
#define asprintf elinks_asprintf
int elinks_asprintf(char **ptr, const char *format, ...);
#endif

#endif
