/* $Id: options.h,v 1.12 2003/09/27 00:32:03 jonas Exp $ */

#ifndef EL__DOCUMENT_OPTIONS_H
#define EL__DOCUMENT_OPTIONS_H

#include "util/color.h"

struct document_options {
	int col, cp, assume_cp, hard_assume;
	int margin;
	int num_links_key;
	int use_document_colours;

	color_t default_fg;
	color_t default_bg;
	color_t default_link;
	color_t default_vlink;

	/* The size of the window */
	int xw, yw;

	unsigned int allow_dark_on_black:1;
	unsigned int tables:1;
	unsigned int table_order:1;
	unsigned int frames:1;
	unsigned int images:1;
	unsigned int display_subs:1;
	unsigned int display_sups:1;
	unsigned int num_links_display:1;
	unsigned int plain:1;

	/* XXX: Everything past this comment is specialy handled by compare_opt() */
	unsigned char *framename;

	/* The position of the window */
	int xp, yp;
};

extern struct document_options *d_opt;

void mk_document_options(struct document_options *doo);
void copy_opt(struct document_options *o1, struct document_options *o2);
int compare_opt(struct document_options *o1, struct document_options *o2);

#endif
