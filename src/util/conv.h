/* $Id: conv.h,v 1.16 2003/07/24 14:36:00 zas Exp $ */

#ifndef EL__UTIL_CONV_H
#define EL__UTIL_CONV_H

#include "lowlevel/ttime.h" /* ttime type */
#include "util/string.h"

static inline unsigned char
upcase(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'z') ch -= 0x20;
	return ch;
}

static inline int
is_safe_in_shell(unsigned char c)
{
	return c == '@' || c == '+' || c == '.' || c == '/' || isA(c);
}


long strtolx(unsigned char *, unsigned char **);

/* Convert a decimal number to hexadecimal (0 <= a <= 15). */
#define hx(a) ((a) >= 10 ? (a) + 'A' - 10 : (a) + '0')

/* Convert an hexadecimal char ([0-9][a-z][A-Z]) to
 * its decimal value (0 <= result <= 15)
 * returns -1 if parameter is not an hexadecimal char. */
#define unhx(a) \
       ( ((a) >= '0' && (a) <= '9') ? (a) - '0' : \
	( ((a) >= 'A' && (a) <= 'F') ? (a) - 'A' + 10 : \
	 ( ((a) >= 'a' && (a) <= 'f') ? (a) - 'a' + 10 : -1 ) \
	) \
       ) \

/* These use granular allocation stuff. */
struct string *add_long_to_string(struct string *string, long number);
struct string *add_knum_to_string(struct string *string, long number);
struct string *add_xnum_to_string(struct string *string, int number);
struct string *add_time_to_string(struct string *string, ttime time);


/* Encoders: */
/* They encode and add to the string. This way we don't need to first allocate
 * and encode a temporary string, add it and then free it. Can be used as
 * backends for encoder. */

/* A simple generic encoder. Should maybe take @replaceable as a string so we
 * could also use if for adding shell safe strings. */
struct string *
add_string_replace(struct string *string, unsigned char *src, int len,
		   unsigned char replaceable, unsigned char replacement);

#define add_optname_to_string(str, src, len) \
	add_string_replace(str, src, len, '.', '*')

/* Maybe a bad name but it is actually the real name, but you may also think of
 * it as adding the decoded option name. */
#define add_real_optname_to_string(str, src, len) \
	add_string_replace(str, src, len, '*', '.')

/* Convert reserved chars to html &#xx */
struct string *add_html_to_string(struct string *string, unsigned char *html, int htmllen);

/* Escapes \ and " with a \ */
struct string *add_quoted_to_string(struct string *string, unsigned char *q, int qlen);

/* Escapes non shell safe chars with '_'. */
struct string *add_shell_safe_to_string(struct string *string, unsigned char *cmd, int cmdlen);


/* These are fast functions to convert integers to string, or to hexadecimal string. */

int elinks_ulongcat(unsigned char *s, unsigned int *slen, unsigned long number,
		    unsigned int width, unsigned char fillchar, unsigned int base,
		    unsigned int upper);

int elinks_longcat(unsigned char *s, unsigned int *slen, long number,
		   unsigned int width, unsigned char fillchar, unsigned int base,
		   unsigned int upper);

/* Type casting is enforced, to shorten calls. --Zas */
/* unsigned long to decimal string */
#define ulongcat(s, slen, number, width, fillchar) \
	elinks_ulongcat((unsigned char *) (s), \
			(unsigned int *) (slen), \
			(unsigned long) (number), \
			(unsigned int) (width), \
			(unsigned char) (fillchar), \
			(unsigned int) 10, \
			(unsigned int) 0)

/* signed long to decimal string */
#define longcat(s, slen, number, width, fillchar) \
	 elinks_longcat((unsigned char *) (s), \
			(unsigned int *) (slen), \
			(long) (number), \
			(unsigned int) (width), \
			(unsigned char) (fillchar), \
			(unsigned int) 10, \
			(unsigned int) 0)

/* unsigned long to hexadecimal string */
#define ulonghexcat(s, slen, number, width, fillchar, upper) \
	elinks_ulongcat((unsigned char *) (s), \
			(unsigned int *) (slen), \
			(unsigned long) (number), \
			(unsigned int) (width), \
			(unsigned char) (fillchar), \
			(unsigned int) 16, \
			(unsigned int) (upper))


/* XXX: Compatibility only. Remove these at some time. --Zas */
#define snprint(str, len, num) ulongcat(str, NULL, num, len, 0);
#define snzprint(str, len, num) longcat(str, NULL, num, len, 0);

unsigned char *encode_shell_safe_url(unsigned char *);
unsigned char *decode_shell_safe_url(unsigned char *);

#endif
