/* $Id: string.h,v 1.6 2002/06/22 21:20:53 pasky Exp $ */

#ifndef EL__UTIL_STRING_H
#define EL__UTIL_STRING_H

/* To these two functions, same remark applies as to copy_string() or
 * straconcat(). */

#include <string.h>
#include "util/memdebug.h"
#include "util/memory.h"

/* Overhead is minimal when DEBUG is not defined, and we will still in fact use
 * normal functions as we have wrappers all around. */

static inline unsigned char *
debug_memacpy(unsigned char *f, int l, unsigned char *src, int len)
{
	unsigned char *m = debug_mem_alloc(f, l, len + 1);

	if (!m) return NULL;

	memcpy(m, src, len);
	m[len] = 0;

	return m;
}
#define memacpy(s, l) debug_memacpy(__FILE__, __LINE__, s, l)

static inline unsigned char *
debug_stracpy(unsigned char *f, int l, unsigned char *src)
{
	if (!src) return NULL;

	return debug_memacpy(f, l, src, (src != DUMMY) ? strlen(src) : 0);
}
#define stracpy(s) debug_stracpy(__FILE__, __LINE__, s)


unsigned char *copy_string(unsigned char **, unsigned char *);
void add_to_strn(unsigned char **, unsigned char *);
unsigned char *straconcat(unsigned char *, ...);

#define ALLOC_GR 0x100 /* must be power of 2 */
#define init_str() init_str_x(__FILE__, __LINE__)
unsigned char *init_str_x(unsigned char *, int);
void add_to_str(unsigned char **, int *, unsigned char *);
void add_bytes_to_str(unsigned char **, int *, unsigned char *, int);
void add_chr_to_str(unsigned char **, int *, unsigned char);
int xstrcmp(unsigned char *, unsigned char *);
int casecmp(unsigned char *, unsigned char *, int);

#ifndef HAVE_STRCASESTR
unsigned char *strcasestr(unsigned char *, unsigned char *);
#endif

unsigned char *safe_strncpy(unsigned char *, const unsigned char *, size_t);


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
