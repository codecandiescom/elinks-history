/* $Id: string.h,v 1.47 2003/07/24 02:01:21 jonas Exp $ */

#ifndef EL__UTIL_STRING_H
#define EL__UTIL_STRING_H

/* To these two functions, same remark applies as to copy_string() or
 * straconcat(). */

#include <string.h>

#include "util/memdebug.h"
#include "util/memory.h"


#ifndef LEAK_DEBUG

/* Autoallocation string constructors: */

/* Note that, contrary to the utilities using the string struct, these
 * functions are NOT granular, thus you can't simply reuse strings allocated by
 * these in add_to_string()-style functions. */

/* Allocates NUL terminated string with @len bytes from @src.
 * If @src == NULL or @len < 0 only one byte is allocated and set it to 0. */
/* Returns the string or NULL on allocation failure. */
unsigned char *memacpy(unsigned char *src, int len);

/* Allocated NUL terminated string with the content of @src. */
unsigned char *stracpy(unsigned char *src);

unsigned char *copy_string(unsigned char **, unsigned char *);

#else /* LEAK_DEBUG */

unsigned char *debug_memacpy(unsigned char *, int, unsigned char *, int);
#define memacpy(s, l) debug_memacpy(__FILE__, __LINE__, s, l)

unsigned char *debug_stracpy(unsigned char *, int, unsigned char *);
#define stracpy(s) debug_stracpy(__FILE__, __LINE__, s)

unsigned char *debug_copy_string(unsigned char *, int, unsigned char **, unsigned char *);
#define copy_string(d, s) debug_copy_string(__FILE__, __LINE__, d, s)

#endif /* LEAK_DEBUG */


/* Concatenates @src to @str. */
/* If reallocation of @str fails @str is not touched. */
void add_to_strn(unsigned char **str, unsigned char *src);

/* Takes a list of strings where the last parameter _must_ be NULL and
 * concatenates them. */
/* Returns the allocated string or NULL on allocation failure. */
/* Example:
 *	unsigned char *abc = straconcat("A", "B", "C", NULL);
 *	if (abc) return;
 *	printf("%s", abc);	-> print "ABC"
 *	mem_free(abc);		-> free memory used by @abc */
unsigned char *straconcat(unsigned char *str, ...);


/* Misc. utility string functions. */

/* Compare two strings, handling correctly @s1 or @s2 being NULL. */
int xstrcmp(unsigned char *s1, unsigned char *s2);

/* Copies at most @len chars into @dst. Ensures null termination of @dst. */
unsigned char *safe_strncpy(unsigned char *dst, const unsigned char *src, size_t len);

/* Trims any starting and ending chars equal to @trim from @str modifying it.
 * If @len != NULL, it is set to length of the new string. */
unsigned char *trim_chars(unsigned char *str, unsigned char trim, int *len);


/* strlcmp() is the middle child of history, everyone is using it differently.
 * On some weird *systems* it seems to be defined (equivalent to strcasecmp()),
 * so we'll better use our #define redir. */

/* This routine compares string @s1 of length @n1 with string @s2 of length
 * @n2.
 *
 * This acts identically to strcmp() but for non-zero-terminated strings,
 * rather than being similiar to strncmp(). That means, it fails if @n1 != @n2,
 * thus you may use it for testing whether @s2 matches *full* @s1, not only its
 * start (which can be a security hole, ie. in the cookies domain checking).
 *
 * @n1 or @n2 may be -1, which is same as strlen(@s[12]) but possibly more
 * effective (in the future ;-). */
/* Returns an integer less than, equal to, or greater than zero if @s1 is
 * found, respectively, to be less than, to match, or be greater than @s2. */
#define strlcmp(a,b,c,d) (errfile = __FILE__, errline = __LINE__, elinks_strlcmp(a,b,c,d))
int elinks_strlcmp(const unsigned char *s1, size_t n1,
		   const unsigned char *s2, size_t n2);

/* Acts identically to strlcmp(), except for being case insensitive. */
#define strlcasecmp(a,b,c,d) (errfile = __FILE__, errline = __LINE__, elinks_strlcasecmp(a,b,c,d))
int elinks_strlcasecmp(const unsigned char *s1, size_t n1,
		       const unsigned char *s2, size_t n2);


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


/* The granularity used for the struct string based utilities. */
/* XXX Must be power of 2 */
#define ALLOC_GR 0x100

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

/* Initializes the passed string struct by preallocating the @source member. */
struct string *init_string(struct string *string);

/* Resets @string and free()s the @source member. */
void done_string(struct string *string);

struct string *add_bytes_to_string(struct string *string, unsigned char *bytes, int length);
struct string *add_to_string(struct string *string, unsigned char *text);
struct string *add_char_to_string(struct string *string, unsigned char character);
struct string *add_string_to_string(struct string *to, struct string *from);

/* Adds each C string to @string until a terminating NULL is met. */
struct string *string_concat(struct string *string, ...);

/* Extends the string with @times number of @character. */
struct string *add_xchar_to_string(struct string *string, unsigned char character, int times);

/* Add printf-style format string to @string. */
struct string *add_format_to_string(struct string *string, unsigned char *format, ...);

#endif /* EL__UTIL_STRING_H */
