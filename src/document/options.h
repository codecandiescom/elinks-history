/* $Id: options.h,v 1.2 2002/05/08 13:55:02 pasky Exp $ */

#ifndef EL__DOCUMENT_OPTIONS_H
#define EL__DOCUMENT_OPTIONS_H

struct document_setup {
	int assume_cp, hard_assume;
	int use_document_colours;
	int avoid_dark_on_black;
	int tables, frames, images;
	int margin;
	int num_links, table_order;
};

#include "document/html/colors.h"

struct document_options {
	int xw, yw; /* size of window */
	int xp, yp; /* pos of window */
	int col, cp, assume_cp, hard_assume;
	int use_document_colours;
	int avoid_dark_on_black;
	/* if you add anything, fix it in compare_opt */
	int tables, frames, images, margin;
	int plain;
	int num_links, table_order;
	struct rgb default_fg;
	struct rgb default_bg;
	struct rgb default_link;
	struct rgb default_vlink;
	unsigned char *framename;
};

extern struct document_options *d_opt;

static inline void ds2do(struct document_setup *ds, struct document_options *doo)
{
	doo->assume_cp = ds->assume_cp;
	doo->hard_assume = ds->hard_assume;
	doo->use_document_colours = ds->use_document_colours;
	doo->avoid_dark_on_black = ds->avoid_dark_on_black;
	doo->tables = ds->tables;
	doo->frames = ds->frames;
	doo->images = ds->images;
	doo->margin = ds->margin;
	doo->num_links = ds->num_links;
	doo->table_order = ds->table_order;
}

void copy_opt(struct document_options *o1, struct document_options *o2);
int compare_opt(struct document_options *o1, struct document_options *o2);

#endif
