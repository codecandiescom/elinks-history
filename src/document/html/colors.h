/* $Id: colors.h,v 1.4 2002/08/30 15:00:02 zas Exp $ */

#ifndef EL__DOCUMENT_HTML_COLORS_H
#define EL__DOCUMENT_HTML_COLORS_H

struct rgb {
	unsigned char r, g, b;
	unsigned char pad;
};

int decode_color(unsigned char *, struct rgb *);
unsigned char * get_color_name(struct rgb *);

int find_nearest_color(struct rgb *, int);
int fg_color(int, int);

#endif
