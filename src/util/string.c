/* String handling functions */
/* $Id: string.c,v 1.53 2003/07/21 04:00:39 jonas Exp $ */

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

#define fatalfl(x) errfile = f, errline = l, elinks_internal(x)
#define fatal(x) internal(x)
#ifdef DEBUG
#define warnfl(x) errfile = f, errline = l, elinks_error(x)
#define warn(x) error(x)
#else
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
		warn("stracpy src=NULL");
		return NULL;
	}

	return memacpy(src, strlen(src));
}

unsigned char *
copy_string(unsigned char **dst, unsigned char *src)
{
	if (!src) {
		warn("copy_string src=NULL");
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
init_str(void)
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


/* libc stub functions */

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
#ifdef DEBUG
	if (!dst) { fatal("safe_strncpy dst=NULL"); return NULL; }
	if (!src) { fatal("safe_strncpy src=NULL"); return NULL; }
	if (dst_size <= 0) { fatal("safe_strncpy dst_size <= 0"); return NULL; }
#endif

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

	return string;
}

void
done_string(struct string *string)
{
	assert(string);
	if_assert_failed { return; }

	if (string->source) {
		mem_free(string->source);
		string->source = NULL;
	}
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

	assert(string && bytes && length);
	if_assert_failed { return NULL; }

	newlength = string->length + length;
	realloc_string(string, newlength);

	memcpy(string->source + string->length, bytes, length);
	string->length = newlength;
	string->source[newlength] = 0;

	return string;
}

struct string *
add_to_string(struct string *string, unsigned char *source)
{
	assert(string && source);
	if_assert_failed { return NULL; }

	return (*source ? add_bytes_to_string(string, source, strlen(source))
			: string);
}

struct string *
add_string_to_string(struct string *string, struct string *from)
{
	assert(string && from);
	if_assert_failed { return NULL; }

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
	assert(string && character);
	if_assert_failed { return NULL; }

	return add_bytes_to_string(string, &character, 1);
}

struct string *
add_xchar_to_string(struct string *string, unsigned char character, int times)
{
	unsigned char buffer[MAX_STR_LEN];

	assert(string && character && times > 0);
	if_assert_failed { return NULL; }

	if (times > MAX_STR_LEN - 1) return NULL;

	memset(buffer, character, times);

	return add_bytes_to_string(string, buffer, times);
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

