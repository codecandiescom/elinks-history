/* String handling functions */
/* $Id: string.c,v 1.70 2003/07/23 15:57:56 pasky Exp $ */

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


/* Autoallocation string constructors */

/* Note that, contrary to init_str() & co, these functions are NOT granular,
 * thus you can't simply reuse strings allocated by these in add_to_str()-style
 * functions. */

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

unsigned char *
debug_copy_string(unsigned char *f, int l, unsigned char **dst,
		  unsigned char *src)
{
	string_assert(f, l, src, "copy_string");
	if_assert_failed { *dst = NULL; return NULL; }

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

	assert(len >= 0);
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
	assert(src);
	if_assert_failed return NULL;

	return memacpy(src, strlen(src));
}

unsigned char *
copy_string(unsigned char **dst, unsigned char *src)
{
	assert(src);
	if_assert_failed { *dst = NULL; return NULL; }

	*dst = mem_alloc(strlen(src) + 1);
	if (*dst) strcpy(*dst, src);

	return *dst;
}
#endif /* LEAK_DEBUG */


void
add_to_strn(unsigned char **s, unsigned char *a)
{
	unsigned char *p;

	assert(*s && a);
	if_assert_failed return;

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

	assert(str);
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



/* Misc. utility string functions. */

/* Compare two strings, handling correctly s1 or s2 being NULL. */
int
xstrcmp(unsigned char *s1, unsigned char *s2)
{
	if (!s1 && !s2) return 0;
	if (!s1) return -1;
	if (!s2) return 1;
	return strcmp(s1, s2);
}

/* Copies at most dst_size chars into dst. Ensures null termination of dst. */
unsigned char *
safe_strncpy(unsigned char *dst, const unsigned char *src, size_t dst_size)
{
	assert(dst && src && dst_size > 0);
	if_assert_failed return NULL;

	strncpy(dst, src, dst_size);
	dst[dst_size - 1] = 0;

	return dst;
}

/* Trim starting and ending chars from a string.
 * Pointer to the string is passed.
 * WARNING: string is modified.
 * If len != NULL, it is set to length of the new string.
 */
inline unsigned char *
trim_chars(unsigned char *s, unsigned char c, int *len)
{
	int l = strlen(s);
	unsigned char *p = s;

	while (*p == c) p++, l--;
	while (l && p[l - 1] == c) p[--l] = '\0';

	memmove(s, p, l + 1);
	if (len) *len = l;

	return s;
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

/* Below are functions similar to add_*_to_str() but using the struct string.
 * It is the preferred way to handle strings now and the old functions will be
 * hopefully removed ASAP.
 *
 * Currently most of the functions use add_bytes_to_string() as a backend but
 * later we can optimize each function. */

struct string *
init_string(struct string *string)
{
	assert(string);
	if_assert_failed { return NULL; }

	string->length = 0;
	string->source = mem_alloc(ALLOC_GR);
	if (!string->source) return NULL;

	*string->source = 0;

	set_string_magic(string);

	return string;
}

void
done_string(struct string *string)
{
	assert(string);
	if_assert_failed { return; }
	check_string_magic(string);

	if (string->source) {
		mem_free(string->source);
		string->source = NULL;
	}
	string->length = 0;
}

#define mask(x)	((x) & ~(ALLOC_GR - 1))

/* General useful to check if @_string_ needs reallocation to fit @_length_. */
#define realloc_string(_string_, _newlength_)				\
	if (mask((_string_)->length) != mask(_newlength_)) {		\
		unsigned char *tmp = (_string_)->source;		\
		tmp = mem_realloc(tmp, mask((_newlength_) + ALLOC_GR));	\
		if (!tmp) return NULL;					\
		(_string_)->source = tmp;				\
	}

struct string *
add_bytes_to_string(struct string *string, unsigned char *bytes, int length)
{
	int newlength;

	assert(string && bytes && length >= 0);
	if_assert_failed { return NULL; }

	check_string_magic(string);

	newlength = string->length + length;
	realloc_string(string, newlength);

	memcpy(string->source + string->length, bytes, length);
	string->source[newlength] = 0;
	string->length = newlength;

	return string;
}

struct string *
add_to_string(struct string *string, unsigned char *source)
{
	assert(string && source);
	if_assert_failed { return NULL; }

	check_string_magic(string);

	return (*source ? add_bytes_to_string(string, source, strlen(source))
			: string);
}

struct string *
add_string_to_string(struct string *string, struct string *from)
{
	assert(string && from);
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

	assert(string);
	if_assert_failed { return NULL; }

	check_string_magic(string);

	va_start(ap, string);
	while ((source = va_arg(ap, unsigned char *)))
		if (*source)
			add_to_string(string, source);

	va_end(ap);

	return string;
}

struct string *
add_char_to_string(struct string *string, unsigned char character)
{
	int newlength;

	assert(string && character);
	if_assert_failed { return NULL; }

	check_string_magic(string);

	newlength = string->length + 1;
	realloc_string(string, newlength);

	string->source[string->length] = character;
	string->source[newlength] = 0;
	string->length = newlength;

	return string;
}

struct string *
add_xchar_to_string(struct string *string, unsigned char character, int times)
{
	int newlength;

	assert(string && character && times > 0);
	if_assert_failed { return NULL; }

	check_string_magic(string);

	newlength = string->length + times;
	realloc_string(string, newlength);

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

	assert(string && format);
	if_assert_failed { return NULL; }

	check_string_magic(string);

	va_start(ap, format);
	VA_COPY(ap2, ap);

	width = vsnprintf(NULL, 0, format, ap2);
	if (width <= 0) return NULL;

	newlength = string->length + width;
	realloc_string(string, newlength);

	vsnprintf(&string->source[string->length], newlength, format, ap);

	va_end(ap);

	string->length = newlength;
	string->source[newlength] = 0;

	return string;
}
