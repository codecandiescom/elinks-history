/* $Id: conv.h,v 1.4 2002/06/21 17:32:48 pasky Exp $ */

#ifndef EL__UTIL_CONV_H
#define EL__UTIL_CONV_H

static inline unsigned char
upcase(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'z') ch -= 0x20;
	return ch;
}

int snprint(unsigned char *, int, unsigned);
int snzprint(unsigned char *, int, int);

void add_num_to_str(unsigned char **, int *, int);
void add_knum_to_str(unsigned char **, int *, int);
long strtolx(unsigned char *, unsigned char **);

unsigned char hx(int);
int unhx(unsigned char);

void add_htmlesc_str(unsigned char **, int *, unsigned char *, int);

#endif
