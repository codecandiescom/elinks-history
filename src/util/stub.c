/* Libc stub functions */
/* $Id: stub.c,v 1.1 2003/07/22 15:59:19 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"


#ifndef HAVE_STRCASECMP
inline int
elinks_strcasecmp(const unsigned char *s1, const unsigned char *s2)
{
	while ((*s1 != '\0')
		 && (upcase(*(unsigned char *)s1) == upcase(*(unsigned char *)s2)))
	{
		s1++;
		s2++;
	}

	return upcase(*(unsigned char *) s1) - upcase(*(unsigned char *) s2);
}
#endif /* !HAVE_STRCASECMP */

#ifndef HAVE_STRNCASECMP
inline int
elinks_strncasecmp(const unsigned char *s1, const unsigned char *s2, size_t len)
{
	if (len == 0)
		return 0;

	while ((len-- != 0)
	       && (upcase(*(unsigned char *)s1) == upcase(*(unsigned char *)s2)))
	{
		if (len == 0 || *s1 == '\0' || *s2 == '\0')
			return 0;
		s1++;
		s2++;
	}

	return upcase(*(unsigned char *) s1) - upcase(*(unsigned char *) s2);
}
#endif /* !HAVE_STRNCASECMP */

#ifndef HAVE_STRCASESTR
/* Stub for strcasestr(), GNU extension */
inline unsigned char *
elinks_strcasestr(const unsigned char *haystack, const unsigned char *needle)
{
	size_t haystack_length = strlen(haystack);
	size_t needle_length = strlen(needle);
	int i;

	if (haystack_length < needle_length)
		return NULL;

	for (i = haystack_length - needle_length + 1; i; i--) {
		if (!strncasecmp(haystack, needle, needle_length))
			return (unsigned char *)haystack;
		haystack++;
	}

	return NULL;
}
#endif

#ifndef HAVE_STRDUP
inline unsigned char *
elinks_strdup(const unsigned char *str)
{
	int str_len = strlen(str);
	unsigned char *new = malloc(str_len + 1);

	if (new) {
		if (str_len) memcpy(new, str, str_len);
		new[str_len] = '\0';
	}
	return new;
}
#endif

#ifndef HAVE_STRERROR
/* Many older systems don't have this, but have the global sys_errlist array
 * instead. */
#if 0
extern int sys_nerr;
extern const char *const sys_errlist[];
#endif
inline const char *
elinks_strerror(int err_no)
{
	if (err_no < 0 || err_no > sys_nerr)
		return (const char *)"Unknown Error";
	else
		return (const char *)sys_errlist[err_no];
}
#endif

#ifndef HAVE_STRSTR
/* From http://www.unixpapa.com/incnote/string.html */
inline char *
elinks_strstr(const char *s, const char *p)
{
	char *sp, *pp;

	for (sp = (char *)s, pp = (char *)p; *sp && *pp; )
	{
		if (*sp == *pp) {
			sp++;
			pp++;
		} else {
			sp = sp - (pp - p) + 1;
			pp = (char *)p;
		}
	}
	return (*pp ? NULL : sp - (pp - p));
}
#endif

#if !defined(HAVE_MEMMOVE) && !defined(HAVE_BCOPY)
/* The memmove() function is a bit rarer than the other memory functions -
 * some systems that have the others don't have this. It is identical to
 * memcpy() but is guaranteed to work even if the strings overlap.
 * Most systems that don't have memmove() do have
 * the BSD bcopy() though some really old systems have neither.
 * Note that bcopy() has the order of the source and destination
 * arguments reversed.
 * From http://www.unixpapa.com/incnote/string.html */
/* Modified for ELinks by Zas. */
inline void *
elinks_memmove(void *d, const void *s, size_t n)
{
	register unsigned char *dst = (unsigned char *) d;
	register unsigned char *src = (unsigned char *) s;

	if (src > dst)
		for (; n > 0; n--)
			*(dst++) = *(src++);
	else
		for (dst += n - 1, src += n - 1;
		     n > 0;
		     n--)
			*(dst--) = *(src--);

	return (void *) dst;
}
#endif


#ifndef HAVE_STPCPY
inline unsigned char *
elinks_stpcpy(unsigned char *dest, unsigned const char *src)
{
	while ((*dest++ = *src++));
	return (dest - 1);
}
#endif

#ifndef HAVE_MEMPCPY
inline void *
elinks_mempcpy(void *dest, const void *src, size_t n)
{
	return (void *) ((unsigned char *) memcpy(dest, src, n) + n);
}
#endif


