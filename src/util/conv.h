/* $Id: conv.h,v 1.2 2002/04/27 21:21:20 pasky Exp $ */

#ifndef EL__UTIL_CONV_H
#define EL__UTIL_CONV_H

unsigned char hx(int);
int unhx(unsigned char);
void add_htmlesc_str(unsigned char **, int *, unsigned char *, int);

#endif
