/* $Id: string.h,v 1.43 2003/07/22 15:59:19 jonas Exp $ */

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

unsigned char *debug_copy_string(unsigned char *, int, unsigned char **, unsigned char *);
#define copy_string(d, s) debug_copy_string(__FILE__, __LINE__, d, s)

#else /* LEAK_DEBUG */

unsigned char *memacpy(unsigned char *, int);
unsigned char *stracpy(unsigned char *);
unsigned char *copy_string(unsigned char **, unsigned char *);

#endif /* LEAK_DEBUG */


void add_to_strn(unsigned char **, unsigned char *);
unsigned char *straconcat(unsigned char *, ...);

int xstrcmp(unsigned char *, unsigned char *);

unsigned char *safe_strncpy(unsigned char *, const unsigned char *, size_t);
unsigned char *trim_chars(unsigned char *, unsigned char, int *);


#define WHITECHAR(x) ((x) == ' ' || ((x) >= ASCII_TAB && (x) <= ASCII_CR))
#define IS_QUOTE(x) ((x) == '"' || (x) == '\'')
#define isA(c) ((c >= 'A' && c <= 'Z') \
		|| (c >= 'a' && c <= 'z') \
		|| (c >= '0' && c <= '9') \
		|| c == '_' || c == '-')


/* String debugging using magic number, it may catches some errors. */
#ifdef DEBUG
#define DEBUG_STRING
#endif

struct string {
#ifdef DEBUG_STRING
	int magic;
#endif
	unsigned char *source;
	int length;
};


#ifdef DEBUG_STRING
#define STRING_MAGIC 0x2E5BF271
#define check_string_magic(x) assertm((x)->magic == STRING_MAGIC, "String magic check failed.")
#define set_string_magic(x) do { (x)->magic = STRING_MAGIC; } while (0)
#define NULL_STRING { STRING_MAGIC, NULL, 0 }
#define INIT_STRING(s, l) { STRING_MAGIC, s, l }
#else
#define check_string_magic(x)
#define set_string_magic(x)
#define NULL_STRING { NULL, 0 }
#define INIT_STRING(s, l) { s, l }
#endif

struct string *init_string(struct string *string);
void done_string(struct string *string);

struct string *add_bytes_to_string(struct string *string, unsigned char *bytes, int length);
struct string *add_to_string(struct string *string, unsigned char *text);
struct string *add_char_to_string(struct string *string, unsigned char character);
struct string *add_string_to_string(struct string *to, struct string *from);

/* Adds each C string to @string until a terminating NULL is met. */
struct string *string_concat(struct string *string, ...);

/* Extends the string with @times number of @character. */
struct string *add_xchar_to_string(struct string *string, unsigned char character, int times);

/* Add printf-like format string to @string. */
struct string *add_format_to_string(struct string *string, unsigned char *format, ...);

#endif /* EL__UTIL_STRING_H */
