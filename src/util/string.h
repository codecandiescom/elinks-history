/* $Id: string.h,v 1.16 2003/04/17 13:25:37 zas Exp $ */

#ifndef EL__UTIL_STRING_H
#define EL__UTIL_STRING_H

/* To these two functions, same remark applies as to copy_string() or
 * straconcat(). */

#include <string.h>
#include "util/memdebug.h"
#include "util/memory.h"

#if !(defined HAVE_STRCHR) && !(defined HAVE_INDEX)
# error You have neither strchr() nor index() function. Please go upgrade your system.
#endif

/* for old BSD systems. */
#ifndef HAVE_STRCHR
# define strchr(a,b) index(a,b)
# define strrchr(a,b) rindex(a,b)
#endif

#ifndef HAVE_STRERROR
char *strerror(int);
#endif

#ifndef HAVE_STRSTR
char *strstr(const char *, const char *);
#endif

#ifndef HAVE_MEMMOVE
#ifdef HAVE_BCOPY
# define memmove(dst, src, n) bcopy(src, dst, n)
#else
char *memmove(char *, const char *, size_t);
#endif
#endif

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

#ifndef HAVE_STRCASECMP
int strcasecmp(const unsigned char *, const unsigned char *);
#endif
#ifndef HAVE_STRNCASECMP
int strncasecmp(const unsigned char *, const unsigned char *, size_t);
#endif
#ifndef HAVE_STRCASESTR
unsigned char *strcasestr(const unsigned char *, const unsigned char *);
#endif
#ifndef HAVE_STRDUP
unsigned char *strdup(const unsigned char *);
#endif
#ifndef HAVE_STPCPY
unsigned char *stpcpy(unsigned char *, unsigned const char *);
#endif
#ifndef HAVE_MEMPCPY
void *mempcpy(void *, const void *, size_t);
#endif


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

#endif
