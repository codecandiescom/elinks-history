/* $Id: string.h,v 1.17 2003/05/01 23:21:29 zas Exp $ */

#ifndef EL__UTIL_STRING_H
#define EL__UTIL_STRING_H

/* To these two functions, same remark applies as to copy_string() or
 * straconcat(). */

#include <string.h>
#include "util/memdebug.h"
#include "util/memory.h"


#define ALLOC_GR 0x100 /* must be power of 2 */

#ifdef LEAK_DEBUG
unsigned char *debug_memacpy(unsigned char *, int, unsigned char *, int);
#define memacpy(s, l) debug_memacpy(__FILE__, __LINE__, s, l)

unsigned char *debug_stracpy(unsigned char *, int, unsigned char *);
#define stracpy(s) debug_stracpy(__FILE__, __LINE__, s)

unsigned char *debug_init_str(unsigned char *, int);
#define init_str() debug_init_str(__FILE__, __LINE__)

unsigned char *debug_copy_string(unsigned char *, int, unsigned char **, unsigned char *);
#define copy_string(d, s) debug_copy_string(__FILE__, __LINE__, d, s)

#else /* LEAK_DEBUG */

unsigned char *memacpy(unsigned char *, int);
unsigned char *stracpy(unsigned char *);
unsigned char *init_str();
unsigned char *copy_string(unsigned char **, unsigned char *);

#endif /* LEAK_DEBUG */


void add_to_strn(unsigned char **, unsigned char *);
unsigned char *straconcat(unsigned char *, ...);

void add_to_str(unsigned char **, int *, unsigned char *);
void add_bytes_to_str(unsigned char **, int *, unsigned char *, int);
void add_chr_to_str(unsigned char **, int *, unsigned char);

int xstrcmp(unsigned char *, unsigned char *);

unsigned char *safe_strncpy(unsigned char *, const unsigned char *, size_t);
unsigned char *trim_chars(unsigned char *, unsigned char, int *);

#define WHITECHAR(x) ((x) == ' ' || ((x) >= 9 && (x) <= 13))
#define IS_QUOTE(x) ((x) == '"' || (x) == '\'')

static inline int
isA(unsigned char c)
{
	return (c >= 'A' && c <= 'Z')
		|| (c >= 'a' && c <= 'z')
		|| (c >= '0' && c <= '9')
		|| c == '_' || c == '-';
}


/*** Our libc... */

#if 0
#ifdef DEBUG
#define USE_LIBC
#endif
#endif

#ifdef USE_LIBC
#undef HAVE_MEMMOVE
#undef HAVE_MEMPCPY
#undef HAVE_STPCPY
#undef HAVE_STRCASECMP
#undef HAVE_STRCASESTR
#undef HAVE_STRCHR
#undef HAVE_STRDUP
#undef HAVE_STRERROR
#undef HAVE_STRNCASECMP
#undef HAVE_STRSTR
#endif

/** strchr() */
#if !(defined HAVE_STRCHR) && !(defined HAVE_INDEX)
# error You have neither strchr() nor index() function. Please go upgrade your system.
#endif

/* for old BSD systems. */
#ifndef HAVE_STRCHR
#undef strchr
#define strchr(a, b) index(a, b)
#undef strrchr
#define strrchr(a, b) rindex(a, b)
#endif

/** strerror() */
#ifndef HAVE_STRERROR
#undef strerror
#define strerror(e) elinks_strerror(e)
char *elinks_strerror(int);
#endif

/** strstr() */
#ifndef HAVE_STRSTR
#undef strstr
#define strstr(a, b) elinks_strstr(a, b)
char *elinks_strstr(const char *, const char *);
#endif

/** memmove() */
#ifndef HAVE_MEMMOVE
#ifdef HAVE_BCOPY
# define memmove(dst, src, n) bcopy(src, dst, n)
#else
#undef memmove
#define memmove(dst, src, n) elinks_memmove(dst, src, n)
char *elinks_memmove(char *, const char *, size_t);
#endif
#endif

/** strcasecmp() */
#ifndef HAVE_STRCASECMP
#undef strcasecmp
#define strcasecmp(a, b) elinks_strcasecmp(a, b)
int elinks_strcasecmp(const unsigned char *, const unsigned char *);
#endif

/** strncasecmp() */
#ifndef HAVE_STRNCASECMP
#undef strncasecmp
#define strncasecmp(a, b, l) elinks_strncasecmp(a, b, l)
int elinks_strncasecmp(const unsigned char *, const unsigned char *, size_t);
#endif

/** strcasestr() */
#ifndef HAVE_STRCASESTR
#undef strcasestr
#define strcasestr(a, b) elinks_strcasestr(a, b)
unsigned char *elinks_strcasestr(const unsigned char *, const unsigned char *);
#endif

/** strdup() */
#ifndef HAVE_STRDUP
#undef strdup
#define strdup(s) elinks_strdup(s)
unsigned char *elinks_strdup(const unsigned char *);
#endif

/* stpcpy() */
#ifndef HAVE_STPCPY
#undef stpcpy
#define stpcpy(d, s) elinks_stpcpy(d, s)
unsigned char *elinks_stpcpy(unsigned char *, unsigned const char *);
#endif

/* mempcpy() */
#ifndef HAVE_MEMPCPY
#undef mempcpy
#define mempcpy(dest, src, n) elinks_mempcpy(dest, src, n)
void *elinks_mempcpy(void *, const void *, size_t);
#endif


#endif /* EL__UTIL_STRING_H */
