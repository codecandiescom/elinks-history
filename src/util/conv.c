/* Conversion functions */
/* $Id: conv.c,v 1.5 2002/06/16 21:22:13 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

#include "links.h"

#include "util/conv.h"


unsigned char
upcase(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'z') ch -= 0x20;
	return ch;
}


/* Return 0 if it went ok, 1 if it doesn't fit there. */
int
snprint(unsigned char *str, int len, unsigned num)
{
	int threshold = 1;

	while (threshold <= num / 10) threshold *= 10;

	for (; len > 0 && threshold; len--) {
		*str = num / threshold + '0';
		str++;

		num %= threshold;
		threshold /= 10;
	}

	*str = '\0';
	return !!threshold;
}

int
snzprint(unsigned char *str, int len, int num)
{
	if (len > 1 && num < 0) {
		*str++ = '-';
		str++;

		num = -num;
		len--;
	}

	return snprint(str, len, num);
}


void
add_num_to_str(unsigned char **str, int *len, int num)
{
	unsigned char buf[64];

	snzprint(buf, 64, num);
	add_to_str(str, len, buf);
}

void
add_knum_to_str(unsigned char **str, int *len, int num)
{
	unsigned char buf[13];

	if (num && (num / (1024 * 1024)) * (1024 * 1024) == num) {
		snzprint(buf, 12, num / (1024 * 1024));
		strcat(buf, "M");

	} else if (num && (num / 1024) * 1024 == num) {
		snzprint(buf, 12, num / 1024);
		strcat(buf, "k");

	} else {
		snzprint(buf, 13, num);
	}

	add_to_str(str, len, buf);
}

long
strtolx(unsigned char *str, unsigned char **end)
{
	long num = strtol(str, (char **) end, 10);
	unsigned char postfix;

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

#define accept_char(x) (isA((x)) || (x) == ' ' || (x) == '.' \
			|| (x) == ':' || (x) == ';')

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
