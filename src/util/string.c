/* String handling functions */
/* $Id: string.c,v 1.6 2002/06/21 20:28:25 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"


/* Autoallocation string constructors */

/* Note that, contrary to init_str() & co, these functions are NOT granular,
 * thus you can't simply reuse strings allocated by these in add_to_str()-style
 * functions. */

unsigned char *
copy_string(unsigned char **dst, unsigned char *src)
{
	if (!src) return NULL;

	*dst = mem_alloc(strlen(src) + 1);
	if (*dst) strcpy(*dst, src);

	return *dst;
}


void
add_to_strn(unsigned char **s, unsigned char *a)
{
	unsigned char *p = mem_realloc(*s, strlen(*s) + strlen(a) + 1);

	if (!p) return;
	strcat(p, a);
	*s = p;
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

	if (!str) return NULL;

	s = stracpy(str);
	if (!s) return NULL;

	len = strlen(s) + 1;

	va_start(ap, str);
	while ((a = va_arg(ap, unsigned char *))) {
		unsigned char *p;

		len += strlen(a);
		p = mem_realloc(s, len);
		if (!p) {
			mem_free(s);
			va_end(ap);
			return NULL;
		}
		s = p;
		strcat(s, a);
	}

	va_end(ap);

	return s;
}


/* Granular autoallocation dynamic string functions */

unsigned char *
init_str_x(unsigned char *file, int line)
{
	unsigned char *p = debug_mem_alloc(file, line, ALLOC_GR);

	if (p) *p = 0;
	return p;
}


void
add_to_str(unsigned char **s, int *l, unsigned char *a)
{
	int ll = strlen(a);

	if ((*l & ~(ALLOC_GR - 1)) != ((*l + ll) & ~(ALLOC_GR - 1))) {
	   unsigned char *p = mem_realloc(*s, (*l + ll + ALLOC_GR)
					      & ~(ALLOC_GR - 1));

	   if (!p) return;
	   *s = p;
	}

	strcpy(*s + *l, a);
   	*l += ll;
}

void
add_bytes_to_str(unsigned char **s, int *l, unsigned char *a, int ll)
{
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

void
add_chr_to_str(unsigned char **s, int *l, unsigned char a)
{
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

int
casecmp(unsigned char *c1, unsigned char *c2, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (upcase(c1[i]) != upcase(c2[i]))
			return 1;

	return 0;
}

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
		if (!casecmp(haystack, needle, needle_length))
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
	strncpy(dst, src, dst_size);
	dst[dst_size - 1] = 0;

	return dst;
}
