/* $Id: conv.h,v 1.6 2003/05/10 01:29:08 zas Exp $ */

#ifndef EL__UTIL_CONV_H
#define EL__UTIL_CONV_H


static inline unsigned char
upcase(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'z') ch -= 0x20;
	return ch;
}


long strtolx(unsigned char *, unsigned char **);

unsigned char hx(int);
int unhx(unsigned char);

/* These use granular allocation stuff like some others funtions
 * in util/string.c, we should perhaps group them. --Zas */
int add_num_to_str(unsigned char **str, int *len, long num);
int add_knum_to_str(unsigned char **str, int *len, long num);
void add_htmlesc_str(unsigned char **, int *, unsigned char *, int);

#endif
