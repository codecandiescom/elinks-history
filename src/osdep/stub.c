/* Libc stub functions */
/* $Id: stub.c,v 1.17 2004/11/13 13:31:54 witekfl Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "osdep/stub.h"
#include "util/conv.h"

/* These stubs are exception to our "Use (unsigned char *)!" rule. This is
 * because the stubbed functions are defined using (char *), and we could get
 * in trouble with this. Or when you use (foo ? strstr() : strcasestr()) and
 * one of these is system and another stub, we're in trouble and get "Pointer
 * type mismatch in conditional expression", game over. */


#define toupper_equal(s1, s2) (toupper(*((char *) s1)) == toupper(*((char *) s2)))
#define toupper_delta(s1, s2) (toupper(*((char *) s1)) - toupper(*((char *) s2)))

#ifndef HAVE_STRCASECMP
inline int
elinks_strcasecmp(const char *s1, const char *s2)
{
	while (*s1 != '\0' && toupper_equal(s1, s2)) {
		s1++;
		s2++;
	}

	return toupper_delta(s1, s2);
}
#endif /* !HAVE_STRCASECMP */

#ifndef HAVE_STRNCASECMP
inline int
elinks_strncasecmp(const char *s1, const char *s2, size_t len)
{
	if (len == 0)
		return 0;

	while (len-- != 0 && toupper_equal(s1, s2)) {
		if (len == 0 || *s1 == '\0' || *s2 == '\0')
			return 0;
		s1++;
		s2++;
	}

	return toupper_delta(s1, s2);
}
#endif /* !HAVE_STRNCASECMP */

#ifndef HAVE_STRCASESTR
/* Stub for strcasestr(), GNU extension */
inline char *
elinks_strcasestr(const char *haystack, const char *needle)
{
	size_t haystack_length = strlen(haystack);
	size_t needle_length = strlen(needle);
	int i;

	if (haystack_length < needle_length)
		return NULL;

	for (i = haystack_length - needle_length + 1; i; i--) {
		if (!strncasecmp(haystack, needle, needle_length))
			return (char *) haystack;
		haystack++;
	}

	return NULL;
}
#endif

#ifndef HAVE_STRDUP
inline char *
elinks_strdup(const char *str)
{
	int str_len = strlen(str);
	char *new = malloc(str_len + 1);

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
		return (const char *) "Unknown Error";
	else
		return (const char *) sys_errlist[err_no];
}
#endif

#ifndef HAVE_STRSTR
/* From http://www.unixpapa.com/incnote/string.html */
inline char *
elinks_strstr(const char *s, const char *p)
{
	char *sp, *pp;

	for (sp = (char *) s, pp = (char *) p; *sp && *pp; )
	{
		if (*sp == *pp) {
			sp++;
			pp++;
		} else {
			sp = sp - (pp - p) + 1;
			pp = (char *) p;
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
	register char *dst = (char *) d;
	register char *src = (char *) s;

	if (!n || src == dst) return (void *) dst;

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
inline char *
elinks_stpcpy(char *dest, const char *src)
{
	while ((*dest++ = *src++));
	return (dest - 1);
}
#endif

#ifndef HAVE_MEMPCPY
inline void *
elinks_mempcpy(void *dest, const void *src, size_t n)
{
	return (void *) ((char *) memcpy(dest, src, n) + n);
}
#endif

#ifndef HAVE_ISDIGIT
inline int
elinks_isdigit(int i)
{
	return i >= '0' && i <= '9';
}
#endif

#ifndef HAVE_MEMRCHR
inline void *
elinks_memrchr(const void *s, int c, size_t n)
{
	char *pos = (char *) s;

	while (n > 0) {
		if (pos[n - 1] == (char) c)
			return (void *) &pos[n - 1];
		n--;
	}

	return NULL;
}
#endif

#ifndef HAVE_RAISE

#if !defined(HAVE_KILL) || !defined(HAVE_GETPID)
#error The raise() stub function requires kill() and getpid()
#endif

int
elinks_raise(int signal)
{
	return(kill(getpid(), signal));
}
#endif
