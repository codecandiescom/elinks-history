/* $Id: colors.h,v 1.1 2002/03/17 11:29:11 pasky Exp $ */

#ifndef EL__COLORS_H
#define EL__COLORS_H

struct rgb {
	unsigned char r, g, b;
	unsigned char pad;
};

int decode_color(unsigned char *, struct rgb *);

#endif
