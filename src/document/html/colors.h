/* $Id: colors.h,v 1.2 2002/03/17 13:54:13 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_COLORS_H
#define EL__DOCUMENT_HTML_COLORS_H

struct rgb {
	unsigned char r, g, b;
	unsigned char pad;
};

int decode_color(unsigned char *, struct rgb *);

#endif
