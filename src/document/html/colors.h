/* $Id: colors.h,v 1.7 2003/06/26 13:55:56 zas Exp $ */

#ifndef EL__DOCUMENT_HTML_COLORS_H
#define EL__DOCUMENT_HTML_COLORS_H

struct rgb {
	unsigned char r, g, b;
	unsigned char pad;
};

int decode_color(unsigned char *, struct rgb *);
unsigned char * get_color_name(struct rgb *);
void color_to_string(struct rgb *, unsigned char *);

int find_nearest_color(struct rgb *, int);
int fg_color(int, int);

void init_colors_lookup(void);
void free_colors_lookup(void);

#endif
