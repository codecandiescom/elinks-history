/* $Id: options.h,v 1.6 2002/09/10 15:12:14 zas Exp $ */

#ifndef EL__DOCUMENT_OPTIONS_H
#define EL__DOCUMENT_OPTIONS_H

#include "document/html/colors.h"

struct document_options {
	int xw, yw; /* size of window */
	int xp, yp; /* pos of window */
	int col, cp, assume_cp, hard_assume;
	int use_document_colours;
	int allow_dark_on_black;
	/* if you add anything, fix it in compare_opt */
	int tables, frames, images, margin;
	int plain;
	int num_links_display;
	int num_links_key;
	int table_order;
	int display_subs;
	int display_sups;
	struct rgb default_fg;
	struct rgb default_bg;
	struct rgb default_link;
	struct rgb default_vlink;
	unsigned char *framename;
};

extern struct document_options *d_opt;

void mk_document_options(struct document_options *doo);
void copy_opt(struct document_options *o1, struct document_options *o2);
int compare_opt(struct document_options *o1, struct document_options *o2);

#endif
