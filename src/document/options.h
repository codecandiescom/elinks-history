/* $Id: options.h,v 1.11 2003/09/26 23:06:06 jonas Exp $ */

#ifndef EL__DOCUMENT_OPTIONS_H
#define EL__DOCUMENT_OPTIONS_H

#include "util/color.h"

struct document_options {
	int col, cp, assume_cp, hard_assume;
	int use_document_colours;
	int allow_dark_on_black;
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

	/* TODO: Having these members as part of the compared ones are to blame
	 * for the massive memory consumption when resizing. They should be
	 * moved after @framename but first they have to somehow be updated
	 * when resizing. --jonas */
	int xw, yw; /* size of window */

	/* XXX: Everything past this comment is specialy handled by compare_opt() */
	unsigned char *framename;
	int xp, yp; /* pos of window */
};

extern struct document_options *d_opt;

void mk_document_options(struct document_options *doo);
void copy_opt(struct document_options *o1, struct document_options *o2);
int compare_opt(struct document_options *o1, struct document_options *o2);

#endif
