/* String handling functions */
/* $Id: string.c,v 1.21 2003/01/20 13:38:14 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

#ifdef DEBUG
#define fatalfl(x) errfile = f, errline = l, int_error(x)
#define fatal(x) internal(x)
#define warnfl(x) error("%s:%d %s\n", f, l, x)
#define warn(x) error("%s:%d %s\n", __FILE__, __LINE__, x)
#else
#define fatalfl(x) error("%s:%d %s\n", f, l, x)
#define fatal(x) error("%s:%d %s\n", __FILE__, __LINE__, x)
#define warnfl(x)
#define warn(x)
#endif


/* Autoallocation string constructors */

/* Note that, contrary to init_str() & co, these functions are NOT granular,
 * thus you can't simply reuse strings allocated by these in add_to_str()-style
 * functions. */

#ifdef LEAK_DEBUG
inline unsigned char *
debug_memacpy(unsigned char *f, int l, unsigned char *src, int len)
{
	unsigned char *m;

	if (len < 0) { warnfl("memacpy len < 1"); len = 0; }

	m = debug_mem_alloc(f, l, len + 1);
	if (m) {
		if (src && len) memcpy(m, src, len);
		m[len] = 0;
	}

	return m;
}

inline unsigned char *
debug_stracpy(unsigned char *f, int l, unsigned char *src)
{
	if (!src) { warnfl("stracpy src=NULL"); return NULL; }

	return debug_memacpy(f, l, src, strlen(src));
}

unsigned char *
debug_copy_string(unsigned char *f, int l, unsigned char **dst,
		  unsigned char *src)
{
	if (!src) {
		warnfl("copy_string src=NULL");
		*dst = NULL;
		return NULL;
	}

	*dst = debug_mem_alloc(f, l, strlen(src) + 1);
	if (*dst) strcpy(*dst, src);

	return *dst;
}

#else /* LEAK_DEBUG */

/* Copy len bytes to an allocated space of len + 1 bytes,
 * last byte is always set to 0.
 * If src == NULL or len < 0 then allocate only one byte
 * and set it to 0.
 * On allocation failure, it returns NULL.
 */
inline unsigned char *
memacpy(unsigned char *src, int len)
{
	unsigned char *m;

#ifdef DEBUG
	if (len < 0) { warn("memacpy len < 0"); len = 0; }
#endif

	m = mem_alloc(len + 1);
	if (m) {
		if (src && len) memcpy(m, src, len);
		m[len] = 0;
	}

	return m;
}

inline unsigned char *
stracpy(unsigned char *src)
{
	if (!src) {
#ifdef DEBUG
		warn("stracpy src=NULL");
#endif
		return NULL;
	}

	return memacpy(src, strlen(src));
}

unsigned char *
copy_string(unsigned char **dst, unsigned char *src)
{
	if (!src) {
#ifdef DEBUG
		warn("copy_string src=NULL");
#endif
		*dst = NULL;
		return NULL;
	}

	*dst = mem_alloc(strlen(src) + 1);
	if (*dst) strcpy(*dst, src);

	return *dst;
}
#endif /* LEAK_DEBUG */


void
add_to_strn(unsigned char **s, unsigned char *a)
{
	unsigned char *p;

#ifdef DEBUG
	if (!*s) { fatal("add_to_strn *s=NULL"); return; }
	if (!a) { fatal("add_to_strn a=NULL"); return; }
#endif

	p = mem_realloc(*s, strlen(*s) + strlen(a) + 1);

	if (p) {
		strcat(p, a);
		*s = p;
	}
}


/* Concatenate all strings parameters. Parameters list must _always_ be
 * terminated by a NULL pointer.  If first parameter is NULL or allocation
 * failure, return NULL.  On success, returns a pointer to a dynamically
 * allocated string.
 *
 * Example:
 * ...
 * unsigned char *s = straconcat("A", "B", "C", NULL);
 * if (!s) return;
 * printf("%s", s); -> print "ABC"
 * mem_free(s); -> free memory used by s
 */
unsigned char *
straconcat(unsigned char *str, ...)
{
	va_list ap;
	unsigned char *a;
	unsigned char *s;
	unsigned int len;

	if (!str) {
#ifdef DEBUG
		fatal("straconcat str=NULL");
#endif
		return NULL;
	}

	s = stracpy(str);
	if (!s) return NULL;

	len = strlen(s) + 1;

	va_start(ap, str);
	while ((a = va_arg(ap, unsigned char *))) {
		if (*a) {
			len += strlen(a);
			s = mem_realloc(s, len);
			if (s)
				strcat(s, a);
			else
				break;
		}
	}

	va_end(ap);

	return s;
}


/* Granular autoallocation dynamic string functions */
#ifdef LEAK_DEBUG
inline unsigned char *
debug_init_str(unsigned char *file, int line)
{
	unsigned char *p = debug_mem_alloc(file, line, ALLOC_GR);

	if (p) *p = 0;

	return p;
}
#else
inline unsigned char *
init_str()
{
	unsigned char *p = mem_alloc(ALLOC_GR);

	if (p) *p = 0;

	return p;
}
#endif

inline void
add_to_str(unsigned char **s, int *l, unsigned char *a)
{
	int ll;

#ifdef DEBUG
	if (!*s) { fatal("add_to_str *s=NULL"); return; }
	if (!a) { fatal("add_to_str a=NULL"); return; }
#endif
	if (!*a) return;

	ll = strlen(a);

	if ((*l & ~(ALLOC_GR - 1)) != ((*l + ll) & ~(ALLOC_GR - 1))) {
	   unsigned char *p = mem_realloc(*s, (*l + ll + ALLOC_GR)
					      & ~(ALLOC_GR - 1));

	   if (!p) return;
	   *s = p;
	}

	strcpy(*s + *l, a);
   	*l += ll;
}

inline void
add_bytes_to_str(unsigned char **s, int *l, unsigned char *a, int ll)
{
#ifdef DEBUG
	if (!*s) { fatal("add_bytes_to_str *s=NULL"); return; }
	if (!a) { fatal("add_bytes_to_str a=NULL"); return; }
	if (ll < 0) { fatal("add_bytes_to_str ll < 0"); return; }
#endif
	if (!ll) return;

	if ((*l & ~(ALLOC_GR - 1)) != ((*l + ll) & ~(ALLOC_GR - 1))) {
		unsigned char *p = mem_realloc(*s, (*l + ll + ALLOC_GR)
			                      & ~(ALLOC_GR - 1));

		if (!p) return;
		*s = p;
	}

	memcpy(*s + *l, a, ll);
	*l += ll;
   	(*s)[*l] = 0;
}

inline void
add_chr_to_str(unsigned char **s, int *l, unsigned char a)
{
#ifdef DEBUG
	if (!*s) { fatal("add_chr_to_str *s=NULL"); return; }
	if (!a) { warn("add_chr_to_str a=0"); }
#endif

	if ((*l & (ALLOC_GR - 1)) == ALLOC_GR - 1) {
		unsigned char *p = mem_realloc(*s, (*l + 1 + ALLOC_GR)
					      & ~(ALLOC_GR - 1));

		if (!p) return;

		*s = p;
	}

	*(*s + *l) = a;
	(*l)++;
	*(*s + *l) = 0;
}


/* String comparison functions */

int
xstrcmp(unsigned char *s1, unsigned char *s2)
{
	if (!s1 && !s2) return 0;
	if (!s1) return -1;
	if (!s2) return 1;
	return strcmp(s1, s2);
}

#ifndef HAVE_STRCASECMP
int
strcasecmp(unsigned char *c1, unsigned char *c2, int len)
{
	int i;

	for (; *c1 && *c2; c1++, c2++)
		if (upcase(c1[i]) != upcase(c2[i]))
			return 1;

	return 0;
}
#endif /* !HAVE_STRCASECMP */

#ifndef HAVE_STRNCASECMP
int
strncasecmp(unsigned char *c1, unsigned char *c2, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (upcase(c1[i]) != upcase(c2[i]))
			return 1;

	return 0;
}
#endif /* !HAVE_STRNCASECMP */

#ifndef HAVE_STRCASESTR
/* Stub for strcasestr(), GNU extension */
unsigned char *
strcasestr(unsigned char *haystack, unsigned char *needle)
{
	size_t haystack_length = strlen(haystack);
	size_t needle_length = strlen(needle);
	int i;

	if (haystack_length < needle_length)
		return NULL;

	for (i = haystack_length - needle_length + 1; i; i--) {
		if (!strncasecmp(haystack, needle, needle_length))
			return haystack;
		haystack++;
	}

	return NULL;
}
#endif


/* Copies at most dst_size chars into dst. Ensures null termination of dst. */
unsigned char *
safe_strncpy(unsigned char *dst, const unsigned char *src, size_t dst_size)
{
#ifdef DEBUG
	if (!dst) { fatal("safe_strncpy dst=NULL"); return NULL; }
	if (!src) { fatal("safe_strncpy src=NULL"); return NULL; }
	if (dst_size <= 0) { fatal("safe_strncpy dst_size <= 0"); return NULL; }
#endif

	strncpy(dst, src, dst_size);
	dst[dst_size - 1] = 0;

	return dst;
}

#ifndef HAVE_STRERROR
/* Many older systems don't have this, but have the global sys_errlist array
 * instead. */
char *
strerror(int errno)
{
	extern int sys_nerr;
	extern char *sys_errlist[];

	if (errno < 0 || errno > sys_nerr)
		return "Unknown Error";
	else
		return sys_errlist[errno];
}
#endif

#ifndef HAVE_STRSTR
/* From http://www.unixpapa.com/incnote/string.html */
char *
strstr(char *s, char *p)
{
	char *sp, *pp;

	for(sp = s, pp = p; *sp && *pp;)
	{
		if (*sp == *pp) {
			sp++;
			pp++;
		} else {
			sp = sp - (pp - p) + 1;
			pp = p;
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
/* XXX: Perhaps not the best place for it. --Zas */
inline char *
memmove(char *dst, char *src, int n)
{
	if (src > dst)
		for ( ; n > 0; n--)
			*(dst++)= *(src++);
	else
		for (dst+= n-1, src+= n-1; n > 0; n--)
			*(dst--)= *(src--);
}
#endif

/* Trim starting and ending chars from a string.
 * WARNING: string is modified, pointer to new start of the
 * string is returned. if len != NULL, it is set to length of
 * trimmed string.
 */
inline unsigned char *trim_chars(unsigned char *s, unsigned char c, int *len)
{
	int l = strlen(s);

	while (*s == c) s++, l--;
	while (l && s[l - 1] == c) s[--l] = '\0';

	if (len) *len = l;
	return s;
}
