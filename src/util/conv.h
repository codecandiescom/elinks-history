/* $Id: conv.h,v 1.13 2003/07/21 04:00:39 jonas Exp $ */

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

unsigned char hx(int);
int unhx(unsigned char);

/* These use granular allocation stuff. */
int add_num_to_str(unsigned char **str, int *len, long num);
int add_knum_to_str(unsigned char **str, int *len, long num);
void add_xnum_to_str(unsigned char **s, int *l, int n);
void add_time_to_str(unsigned char **s, int *l, ttime t);
void add_htmlesc_str(unsigned char **, int *, unsigned char *, int);

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
