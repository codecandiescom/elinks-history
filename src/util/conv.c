/* Conversion functions */
/* $Id: conv.c,v 1.16 2003/05/10 01:29:08 zas Exp $ */

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


/* TODO: Move it to string.c. --Zas */
int
add_num_to_str(unsigned char **str, int *len, long num)
{
	int ret;
	unsigned char t[32];
	int tlen = 0;

	ret = longcat(&t, &tlen, num, sizeof(t) - 1, 0);
	if (ret < 2 && tlen) {
		if ((*len & ~(ALLOC_GR - 1))
		    != ((*len + tlen) & ~(ALLOC_GR - 1))) {
		   	unsigned char *p = mem_realloc(*str,
					               (*len + tlen + ALLOC_GR)
				 		       & ~(ALLOC_GR - 1));

	   		if (!p) return 2;
	   		*str = p;
		}

		memcpy(*str + *len, t, tlen + 1);
		*len += tlen;
	}

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

	if (ret < 2 && tlen) {
		if ((*len & ~(ALLOC_GR - 1))
		    != ((*len + tlen) & ~(ALLOC_GR - 1))) {
		   	unsigned char *p = mem_realloc(*str,
					               (*len + tlen + ALLOC_GR)
				 		       & ~(ALLOC_GR - 1));

	   		if (!p) return 2;
	   		*str = p;
		}

		memcpy(*str + *len, t, tlen + 1);
		*len += tlen;
	}

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
