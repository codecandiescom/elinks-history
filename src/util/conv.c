/* Conversion functions */
/* $Id: conv.c,v 1.51 2004/01/03 15:07:43 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/charsets.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/string.h"



/* This function takes string @s and stores the @number (of a result width
 * @width) in string format there, starting at position [*@slen]. If the number
 * would take more space than @width, it is truncated and only the _last_
 * digits of it are inserted to the string. If the number takes less space than
 * @width, it is padded by @fillchar from left.
 * @base defined which base should be used (10, 16, 8, 2, ...)
 * @upper selects either hexa uppercased chars or lowercased chars.
 *
 * A NUL char is always added at the end of the string. @s must point to a
 * sufficiently large memory space, at least *@slen + @width + 1.
 *
 * Examples:
 *
 * elinks_ulongcat(s, NULL, 12345, 4, 0, 10, 0) : s = "2345"
 * elinks_ulongcat(s, NULL, 255, 4, '*', 16, 1) : s = "**FF"
 * elinks_ulongcat(s, NULL, 123, 5, '0', 10, 0) : s = "00123"
 *
 * Note that this function exists to provide a fast and efficient, however
 * still quite powerful alternative to sprintf(). It is optimized for speed and
 * is *MUCH* faster than sprintf(). If you can use it, use it ;-). But do not
 * get too enthusiastic, do not use it in cases where it would break i18n.
 */
/* The function returns 0 if OK or width needed for the whole number to fit
 * there, if it had to be truncated. A negative value signs an error. */
int inline
elinks_ulongcat(unsigned char *s, unsigned int *slen,
		unsigned long number, unsigned int width,
		unsigned char fillchar, unsigned int base,
		unsigned int upper)
{
	static unsigned char unum[]= "0123456789ABCDEF";
	static unsigned char lnum[]= "0123456789abcdef";
	unsigned char *to_num = (unsigned char *) (upper ? &unum : &lnum);
	unsigned int start = slen ? *slen : 0;
	register unsigned int nlen = 1; /* '0' is one char, we can't have less. */
	unsigned int pos = start; /* starting position of the number */
	unsigned long q = number;
	int ret = 0;

	if (width < 1 || !s || base < 2 || base > 16) return -1;

	/* Count the length of the number in chars. */
	while (q > (base - 1)) {
		nlen++;
		q /= base;
	}

	/* If max. width attained, truncate. */
	if (nlen > width) {
		ret = nlen;
		nlen = width;
	}

	if (slen) *slen += nlen;

	/* Fill left space with fillchar. */
	if (fillchar) {
		/* ie. width = 4 nlen = 2 -> pad = 2 */
		register unsigned int pad = width - nlen;

		if (pad > 0) {
			/* Relocate the start of number. */
			if (slen) *slen += pad;
			pos += pad;

			/* Pad. */
			while (pad > 0) s[--pad + start] = fillchar;
		}
	}

	s[pos + nlen] = '\0';

	/* Now write number starting from end. */
	while (nlen > 0) {
		s[--nlen + pos] = to_num[(number % base)];
		number /= base;
	}

	return ret;
}

/* Similar to elinks_ulongcat() but for long number. */
int inline
elinks_longcat(unsigned char *s, unsigned int *slen,
	       long number, unsigned int width,
	       unsigned char fillchar, unsigned int base,
	       unsigned int upper)
{
	unsigned char *p = s;

	if (number < 0 && width > 0) {
		if (slen) p[(*slen)++] = '-';
		else *(p++) = '-';
		number = -number;
		width--;
	}

	return elinks_ulongcat(p, slen, number, width, fillchar, base, upper);
}


struct string *
add_long_to_string(struct string *string, long number)
{
	unsigned char buffer[32];
	int length = 0;
	int width;

	assert(string);
	if_assert_failed { return NULL; }

	width = longcat(buffer, &length, number, sizeof(buffer) - 1, 0);
	if (width < 0 || !length) return NULL;

	return add_bytes_to_string(string, buffer, length);
}

struct string *
add_knum_to_string(struct string *string, long num)
{
	int ret;
	unsigned char t[32];
	int tlen = 0;

	if (num && (num / (1024 * 1024)) * (1024 * 1024) == num) {
		ret = longcat(&t, &tlen, num / (1024 * 1024), sizeof(t) - 2, 0);
		t[tlen++] = 'M';
		t[tlen] = '\0';
	} else if (num && (num / 1024) * 1024 == num) {
		ret = longcat(&t, &tlen, num / 1024, sizeof(t) - 2, 0);
		t[tlen++] = 'k';
		t[tlen] = '\0';
	} else {
		ret = longcat(&t, &tlen, num, sizeof(t) - 1, 0);
	}

	if (ret < 0 || !tlen) return NULL;

	add_bytes_to_string(string, t, tlen);

	return string;
}

struct string *
add_xnum_to_string(struct string *string, int xnum)
{
	unsigned char suff[3] = "\0i";
	int d = -1;

	/* XXX: I don't completely like the computation of d here. --pasky */
	/* Mebi (Mi), 2^20 */
	if (xnum >= 1024*1024)  {
		suff[0] = 'M';
		d = (xnum / (int)((int)(1024*1024)/(int)10)) % 10;
		xnum /= 1024*1024;
	/* Kibi (Ki), 2^10 */
	} else if (xnum >= 1024) {
		suff[0] = 'K';
		d = (xnum / (int)((int)1024/(int)10)) % 10;
		xnum /= 1024;
	}
	add_long_to_string(string, xnum);

	if (xnum < 10 && d != -1) {
		add_char_to_string(string, '.');
		add_long_to_string(string, d);
	}
	add_char_to_string(string, ' ');

	if (suff[0]) add_to_string(string, suff);
	add_char_to_string(string, 'B');
	return string;
}

struct string *
add_time_to_string(struct string *string, ttime time)
{
	unsigned char q[64];
	int qlen = 0;

	time /= 1000;
	time &= 0xffffffff;

	if (time < 0) time = 0;

	/* Days */
	if (time >= (24 * 3600)) {
		ulongcat(q, &qlen, (time / (24 * 3600)), 5, 0);
		q[qlen++] = 'd';
		q[qlen++] = ' ';
	}

	/* Hours and minutes */
	if (time >= 3600) {
		time %= (24 * 3600);
		ulongcat(q, &qlen, (time / 3600), 4, 0);
		q[qlen++] = ':';
		ulongcat(q, &qlen, ((time / 60) % 60), 2, '0');
	} else {
		/* Only minutes */
		ulongcat(q, &qlen, (time / 60), 2, 0);
	}

	/* Seconds */
	q[qlen++] = ':';
	ulongcat(q, &qlen, (time % 60), 2, '0');

	add_to_string(string, q);
	return string;
}


/* Encoders and string changers */

struct string *
add_string_replace(struct string *string, unsigned char *src, int len,
		   unsigned char replaceable, unsigned char replacement)
{
	int oldlength = string->length;

	if (!add_bytes_to_string(string, src, len))
		return NULL;

	for (src = string->source + oldlength; len; len--, src++)
		if (*src == replaceable)
			*src = replacement;

	return string;
}

struct string *
add_html_to_string(struct string *string, unsigned char *src, int len)
{

#ifndef HAVE_ISALNUM
#define isalphanum(q) isA(q)
#else
#define isalphanum(q) (isalnum(q) || (q) == '-' || (q) == '_')
#endif

	for (; len; len--, src++) {
		if (isalphanum(*src) || *src == ' '
		    || *src == '.' || *src == ':' || *src == ';') {
			add_bytes_to_string(string, src, 1);
		} else {
			add_bytes_to_string(string, "&#", 2);
			add_long_to_string(string, (long) *src);
			add_char_to_string(string, ';');
		}
	}

#undef isalphanum

	return string;
}

/* TODO Optimize later --pasky */
struct string *
add_quoted_to_string(struct string *string, unsigned char *src, int len)
{
	for (; len; len--, src++) {
		if (*src == '"' || *src == '\\')
			add_char_to_string(string, '\\');
		add_char_to_string(string, *src);
	}

	return string;
}

struct string *
add_shell_safe_to_string(struct string *string, unsigned char *cmd, int cmdlen)
{
	for (; cmdlen; cmdlen--, cmd++) {
		if (is_safe_in_shell(*cmd)) {
			add_char_to_string(string, *cmd);
		} else {
			add_char_to_string(string, '%');
			add_char_to_string(string, hx((*cmd & 0xf0) >> 4));
			add_char_to_string(string, hx(*cmd & 0x0f));
		}
	}

	return string;
}


long
strtolx(unsigned char *str, unsigned char **end)
{
	long num;
	unsigned char postfix;

	errno = 0;
	num = strtol(str, (char **) end, 10);
	if (errno) return 0;
	if (!*end) return num;

	postfix = upcase(**end);
	if (postfix == 'K') {
		(*end)++;
		if (num < -MAXINT / 1024) return -MAXINT;
		if (num > MAXINT / 1024) return MAXINT;
		return num * 1024;
	}

	if (postfix == 'M') {
		(*end)++;
		if (num < -MAXINT / (1024 * 1024)) return -MAXINT;
		if (num > MAXINT / (1024 * 1024)) return MAXINT;
		return num * (1024 * 1024);
	}

	return num;
}

/* This is _NOT_ for what do you think it's for! We use this to make URL
 * shell-safe, nothing more. */
unsigned char *
encode_shell_safe_url(unsigned char *url)
{
	struct string u;

	if (!init_string(&u)) return NULL;

	for (; *url; url++) {
		if (is_safe_in_shell(*url))
			add_char_to_string(&u, *url);
		else {
			add_char_to_string(&u, '=');
			add_char_to_string(&u, hx(*url >> 4));
			add_char_to_string(&u, hx(*url & 0xf));
			add_char_to_string(&u, '=');
		}
	}

	return u.source;
}

/* This is _NOT_ for what do you think it's for! We use this to recover from
 * making URL shell-safe, nothing more. */
unsigned char *
decode_shell_safe_url(unsigned char *url)
{
	size_t url_len = strlen(url);
	struct string u;

	if (!init_string(&u)) return NULL;

	for (; *url; url++, url_len--) {
		if (url_len < 4 || url[0] != '=' || unhx(url[1]) == -1
		    || unhx(url[2]) == -1 || url[3] != '=') {
			add_char_to_string(&u, *url);
		} else {
			add_char_to_string(&u, (unhx(url[1]) << 4) + unhx(url[2]));
			url += 3;
			url_len -= 3;
		}
	}

	return u.source;
}

int
month2num(const unsigned char *month)
{
	switch (month[0]) {
	case 'j': /* jan, jun, jul */
		if (month[1] == 'a') {
			if (month[2] == 'n') return 0; /* jan */
			return -1;
		}
		if (month[1] == 'u') {
			if (month[2] == 'n') return 5; /* jun */
			if (month[2] == 'l') return 6; /* jul */
		}
		return -1;
	case 'm': /* mar, may */
		if (month[1] == 'a') {
			if (month[2] == 'r') return 2; /* mar */
			if (month[2] == 'y') return 4; /* may */
		}
		return -1;
	case 'a': /* apr, aug */
		if (month[1] == 'p') {
			if (month[2] == 'r') return 3; /* apr */
			return -1;
		}
		if (month[1] == 'u' && month[2] == 'g') return 7; /* aug */
		return -1;
	case 's':
		if (month[1] == 'e' && month[2] == 'p') return 8; /* sep */
		return -1;
	case 'o':
		if (month[1] == 'c' && month[2] == 't') return 9; /* oct */
		return -1;
	case 'n':
		if (month[1] == 'o' && month[2] == 'v') return 10; /* nov */
		return -1;
	case 'd':
		if (month[1] == 'e' && month[2] == 'c') return 11; /* dec */
		return -1;
	case 'f':
		if (month[1] == 'e' && month[2] == 'b') return 1; /* feb */
		return -1;
	default:
		return -1;
	}
}

/* This function drops control chars, nbsp char and limit the number of consecutive
 * space chars to one. It modifies its argument. */
void
clr_spaces(unsigned char *str)
{
	unsigned char *s;
	unsigned char *dest = str;

	assert(str);

	for (s = str; *s; s++)
		if (*s < ' ' || *s == NBSP_CHAR) *s = ' ';

	for (s = str; *s; s++) {
		if (*s == ' ' && (dest == str || s[1] == ' ' || !s[1]))
			continue;

		*dest++ = *s;
	}

	*dest = '\0';
}
