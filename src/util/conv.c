/* Conversion functions */
/* $Id: conv.c,v 1.38 2003/05/15 12:53:59 zas Exp $ */

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

#include "util/conv.h"
#include "util/string.h"



/* This function takes string @s and stores the @number (of a result width
 * @width) in string format there, starting at position [*@slen]. If the number
 * would take more space than @width, it is truncated and only the _last_
 * digits of it are inserted to the string. If the number takes less space than
 * @width, it is padded by @fillchar from left.
 *
 * A NUL char is always added at the end of the string. @s must point to a
 * sufficiently large memory space, at least *@slen + @width + 1.
 *
 * Example:
 *
 * ulongcat(s, NULL, 12345, 4, 0) : s = "2345"
 *
 * ulongcat(s, NULL, 123, 5, '0') : s = "00123"
 *
 * Note that this function exists to provide a fast and effecient, however
 * still quite powerful alternative to sprintf(). It is optimized for speed and
 * is *MUCH* faster than sprintf(). If you can use it, use it ;-). But do not
 * get too enthusiastic, do not use it in cases where it would break i18n.
 */
/* The function returns 0 if OK or width needed for the whole number to fit
 * there, if it had to be truncated. A negative value signs an error. */
/* TODO: Align to right, left, center... --Zas */
int inline
elinks_ulongcat(unsigned char *s, unsigned int *slen,
		unsigned long number, unsigned int width,
		unsigned char fillchar)
{
	unsigned int start = slen ? *slen : 0;
	unsigned int nlen = 1; /* '0' is one char, we can't have less. */
	unsigned int pos = start; /* starting position of the number */
	unsigned long q = number;
	int ret = 0;

	if (width < 1 || !s) return -1;

	/* Count the length of the number in chars. */
	/* 10 -> nlen = 2, 100 -> nlen = 3, ... */
	while (q > 9) {
		nlen++;
		q /= 10;
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
		unsigned int pad = width - nlen;

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
		s[--nlen + pos] = '0' + (number % 10);
		number /= 10;
	}

	return ret;
}

int inline
elinks_longcat(unsigned char *s, unsigned int *slen,
	       long number, unsigned int width,
	       unsigned char fillchar)
{
	unsigned char *p = s;

	if (number < 0 && width > 0) {
		if (slen) p[(*slen)++] = '-';
		else *(p++) = '-';
		number = -number;
		width--;
	}

	return elinks_ulongcat(p, slen, number, width, fillchar);
}

/* This function is similar to elinks_ulongcat() but convert a long to
 * hexadecimal format.
 * An additionnal parameter 'upper' permits to choose between
 * uppercased and lowercased hexa numbers. */
int inline
elinks_ulonghexcat(unsigned char *s, unsigned int *slen,
		   unsigned long number, unsigned int width,
		   unsigned char fillchar, unsigned int upper)
{
	static unsigned char uhex[]= "0123456789ABCDEF";
	static unsigned char lhex[]= "0123456789abcdef";
	unsigned char *hex = (unsigned char *) (upper ? &uhex : &lhex);
	unsigned int start = slen ? *slen : 0;
	unsigned int nlen = 1; /* '0' is one char, we can't have less. */
	unsigned int pos = start; /* starting position of the number */
	unsigned long q = number;
	int ret = 0;

	if (width < 1 || !s) return -1;

	/* Count the length of the number in chars. */
	while (q > 15) {
		nlen++;
		q /= 16;
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
		unsigned int pad = width - nlen;

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
		s[--nlen + pos] = hex[(number % 16)];
		number /= 16;
	}

	return ret;
}


int
add_num_to_str(unsigned char **str, int *len, long num)
{
	int ret;
	unsigned char t[32];
	int tlen = 0;

	ret = longcat(&t, &tlen, num, sizeof(t) - 1, 0);
	if (ret < 0 || !tlen) return ret;

	add_bytes_to_str(str, len, t, tlen);

	return ret;
}

int
add_knum_to_str(unsigned char **str, int *len, long num)
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

	if (ret < 0 || !tlen) return ret;

	add_bytes_to_str(str, len, t, tlen);

	return ret;
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


unsigned char
hx(int a)
{
	return a >= 10 ? a + 'A' - 10 : a + '0';
}

int
unhx(unsigned char a)
{
	if (a >= '0' && a <= '9') return a - '0';
	if (a >= 'A' && a <= 'F') return a - 'A' + 10;
	if (a >= 'a' && a <= 'f') return a - 'a' + 10;
	return -1;
}


/* Convert chars to html &#xx */
void
add_htmlesc_str(unsigned char **str, int *strl,
		unsigned char *ostr, int ostrl)
{

#ifdef HAVE_ISALNUM
#define accept_char(x) (isalnum((x)) || (x) == '-' || (x) == '_' \
			|| (x) == ' ' || (x) == '.' \
			|| (x) == ':' || (x) == ';')

#else
#define accept_char(x) (isA((x)) || (x) == ' ' || (x) == '.' \
			|| (x) == ':' || (x) == ';')
#endif
	for (; ostrl; ostrl--, ostr++) {
		if (accept_char(*ostr)) {
			add_chr_to_str(str, strl, *ostr);
		} else {
			add_to_str(str, strl, "&#");
			add_num_to_str(str, strl, (int) *ostr);
			add_chr_to_str(str, strl, ';');
		}
	}

#undef accept_char

}
