/* $Id: colors.h,v 1.3 2002/08/07 02:56:59 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_COLORS_H
#define EL__DOCUMENT_HTML_COLORS_H

struct rgb {
	unsigned char r, g, b;
	unsigned char pad;
};

int decode_color(unsigned char *, struct rgb *);

int find_nearest_color(struct rgb *, int);
int fg_color(int, int);

#endif
