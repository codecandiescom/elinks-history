/* String handling functions */
/* $Id: string.c,v 1.87 2003/12/12 14:36:56 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/snprintf.h"


/* This file looks to be slowly being overloaded by a lot of various stuff,
 * like memory managment, stubs, tools, granular and non-granular strings,
 * struct string object... Perhaps util/memory.* and util/stubs.* (stubs.h
 * probably included in elinks.h, it's important enough) would be nice to
 * have. --pasky */


#define string_assert(f, l, x, o) \
	if ((assert_failed = !(x))) { \
		errfile = f, errline = l, \
		elinks_internal("[" o "] assertion " #x " failed!"); \
	}

#ifdef LEAK_DEBUG

inline unsigned char *
debug_memacpy(unsigned char *f, int l, unsigned char *src, int len)
{
	unsigned char *m;

	string_assert(f, l, len >= 0, "memacpy");
	if_assert_failed len = 0;

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
	string_assert(f, l, src, "stracpy");
	if_assert_failed return NULL;

	return debug_memacpy(f, l, src, strlen(src));
}

#else /* LEAK_DEBUG */

inline unsigned char *
memacpy(unsigned char *src, int len)
{
	unsigned char *m;

	assertm(len >= 0, "[memacpy]");
	if_assert_failed { len = 0; }

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
	assertm(src, "[stracpy]");
	if_assert_failed return NULL;

	return memacpy(src, strlen(src));
}

#endif /* LEAK_DEBUG */


void
add_to_strn(unsigned char **dst, unsigned char *src)
{
	unsigned char *newdst;
	int dstlen;
	int srclen;

	assertm(*dst && src, "[add_to_strn]");
	if_assert_failed return;

	dstlen = strlen(*dst);
	srclen = strlen(src) + 1; /* Include the NUL char! */
	newdst = mem_realloc(*dst, dstlen + srclen);
	if (newdst) {
		memcpy(newdst + dstlen, src, srclen);
		*dst = newdst;
	}
}


unsigned char *
straconcat(unsigned char *str, ...)
{
	va_list ap;
	unsigned char *a;
	unsigned char *s;
	unsigned int len;

	assertm(str, "[straconcat]");
	if_assert_failed { return NULL; }

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



int
xstrcmp(register unsigned char *s1, register unsigned char *s2)
{
	if (!s1) return -!!s2;
	if (!s2) return 1;
	return strcmp(s1, s2);
}

unsigned char *
safe_strncpy(unsigned char *dst, const unsigned char *src, size_t dst_size)
{
	assertm(dst && src && dst_size > 0, "[safe_strncpy]");
	if_assert_failed return NULL;

	strncpy(dst, src, dst_size);
	dst[dst_size - 1] = 0;

	return dst;
}

#define strlcmp_device(c,s1,n1,s2,n2,t1,t2) {\
	size_t p, n; \
 \
	/* XXX: The return value is inconsistent. Hrmpf. Making it consistent
	 * would make the @n1 != @n2 case significantly more expensive, though.
	 * So noone should better rely on the return value actually meaning
	 * anything quantitively. --pasky */ \
 \
	string_assert(errfile, errline, s1 && s2, c); \
 \
	/* n1,n2 is unsigned, so don't assume -1 < 0 ! >:) */ \
 \
	/* TODO: Don't precompute strlen()s but rather make the loop smarter.
	 * --pasky */ \
	if (n1 == -1) n1 = strlen(s1); \
	if (n2 == -1) n2 = strlen(s2); \
 \
	string_assert(errfile, errline, n1 >= 0 && n2 >= 0, c); \
 \
	if (n1 != n2) return n1 - n2; \
 \
	for (p = 0, n = n1; p < n && s1[p] && s2[p]; p++) \
		if (t1 != t2) \
			return t1 - t2; \
 \
	return 0; \
}

int
elinks_strlcmp(const unsigned char *s1, size_t n1,
		const unsigned char *s2, size_t n2)
{
	strlcmp_device("strlcmp", s1, n1, s2, n2, s1[p], s2[p]);
}

int
elinks_strlcasecmp(const unsigned char *s1, size_t n1,
		   const unsigned char *s2, size_t n2)
{
	strlcmp_device("strlcasecmp", s1, n1, s2, n2, upcase(s1[p]), upcase(s2[p]));
}


/* The new string utilities: */

/* TODO Currently most of the functions use add_bytes_to_string() as a backend
 *	instead we should optimize each function. */

inline struct string *
init_string(struct string *string)
{
	assertm(string, "[init_string]");
	if_assert_failed { return NULL; }

	string->length = 0;
	string->source = mem_alloc(STRING_GRANULARITY + 1);
	if (!string->source) return NULL;

	*string->source = 0;

	set_string_magic(string);

	return string;
}

inline void
done_string(struct string *string)
{
	assertm(string, "[done_string]");
	if_assert_failed { return; }
	check_string_magic(string);

	if (string->source) {
		mem_free(string->source);
	}
	/* Blast everything including the magic */
	memset(string, 0, sizeof(struct string));
}

inline struct string *
add_to_string(struct string *string, unsigned char *source)
{
	assertm(string && source, "[add_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	return (*source ? add_bytes_to_string(string, source, strlen(source))
			: string);
}

inline struct string *
add_string_to_string(struct string *string, struct string *from)
{
	assertm(string && from, "[add_string_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);
	check_string_magic(from);

	return (*from->source
		? add_bytes_to_string(string, from->source, from->length)
		: string);
}

struct string *
string_concat(struct string *string, ...)
{
	va_list ap;
	unsigned char *source;

	assertm(string, "[string_concat]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	va_start(ap, string);
	while ((source = va_arg(ap, unsigned char *)))
		if (*source)
			add_to_string(string, source);

	va_end(ap);

	return string;
}

inline struct string *
add_char_to_string(struct string *string, unsigned char character)
{
	int newlength;

	assertm(string && character, "[add_char_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	newlength = string->length + 1;
	if (!realloc_string(string, newlength))
		return NULL;

	string->source[string->length] = character;
	string->source[newlength] = 0;
	string->length = newlength;

	return string;
}

inline struct string *
add_xchar_to_string(struct string *string, unsigned char character, int times)
{
	int newlength;

	assertm(string && character && times >= 0, "[add_xchar_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	if (times == 0) return string;

	newlength = string->length + times;
	if (!realloc_string(string, newlength))
		return NULL;

	memset(string->source + string->length, character, times);
	string->source[newlength] = 0;
	string->length = newlength;

	return string;
}

/* Add printf-like format string to @string. */
struct string *
add_format_to_string(struct string *string, unsigned char *format, ...)
{
	int newlength;
	int width;
	va_list ap;
	va_list ap2;

	assertm(string && format, "[add_format_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	va_start(ap, format);
	VA_COPY(ap2, ap);

	width = vsnprintf(NULL, 0, format, ap2);
	if (width <= 0) return NULL;

	newlength = string->length + width;
	if (!realloc_string(string, newlength))
		return NULL;

	vsnprintf(&string->source[string->length], width + 1, format, ap);

	va_end(ap);

	string->length = newlength;
	string->source[newlength] = 0;

	return string;
}
