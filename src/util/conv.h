/* $Id: conv.h,v 1.3 2002/06/16 21:22:13 pasky Exp $ */

#ifndef EL__UTIL_CONV_H
#define EL__UTIL_CONV_H

unsigned char upcase(unsigned char);

int snprint(unsigned char *, int, unsigned);
int snzprint(unsigned char *, int, int);

void add_num_to_str(unsigned char **, int *, int);
void add_knum_to_str(unsigned char **, int *, int);
long strtolx(unsigned char *, unsigned char **);

unsigned char hx(int);
int unhx(unsigned char);

void add_htmlesc_str(unsigned char **, int *, unsigned char *, int);

#endif
