/* $Id: options.h,v 1.9 2003/09/08 21:07:56 jonas Exp $ */

#ifndef EL__DOCUMENT_OPTIONS_H
#define EL__DOCUMENT_OPTIONS_H

#include "util/color.h"

/* XXX: If you add anything, fix it in compare_opt */
struct document_options {
	int xw, yw; /* size of window */
	int xp, yp; /* pos of window */
	int col, cp, assume_cp, hard_assume;

	/* Color options. */
	int use_document_colours;
	int allow_dark_on_black;
	int color_size;

	int tables, frames, images, margin;
	int plain;
	int num_links_display;
	int num_links_key;
	int table_order;
	int display_subs;
	int display_sups;
	color_t default_fg;
	color_t default_bg;
	color_t default_link;
	color_t default_vlink;
	unsigned char *framename;
};

extern struct document_options *d_opt;

void mk_document_options(struct document_options *doo);
void copy_opt(struct document_options *o1, struct document_options *o2);
int compare_opt(struct document_options *o1, struct document_options *o2);

#endif
