/* $Id: options.h,v 1.20 2003/10/17 20:57:29 jonas Exp $ */

#ifndef EL__DOCUMENT_OPTIONS_H
#define EL__DOCUMENT_OPTIONS_H

#include "util/color.h"

/* This mostly acts as a option cache so rendering will be faster. However it
 * is also used to validate and invalidate documents in the format cache as to
 * whether they satisfy the current state of the document options. */
struct document_options {
	int color_mode, cp, assume_cp, hard_assume;
	int margin;
	int num_links_key;
	int use_document_colours;
	int meta_link_display;

	/* The default (fallback) colors. */
	color_t default_fg;
	color_t default_bg;
	color_t default_link;
	color_t default_vlink;

	/* The size of the window. */
	/* This controls how wide tables can be rendered and so on thus also is
	 * to blame for the extra memory consumption when resizing because all
	 * documents has to be rerendered. */
	int xw, yw;

	/* XXX: Keep boolean options grouped to save padding */
	/* HTML stuff */
	unsigned int tables:1;
	unsigned int table_order:1;
	unsigned int frames:1;
	unsigned int images:1;

	/* Color model/optimizations */
	/* TODO: Store the color_flags options in an enum variable. */
	unsigned int allow_dark_on_black:1;
	unsigned int ensure_contrast:1;
	unsigned int underline:1;
	unsigned int display_subs:1;
	unsigned int display_sups:1;
	unsigned int invert_active_link:1;
	unsigned int underline_links:1;

	/* Link navigation */
	unsigned int num_links_display:1;
	unsigned int use_tabindex:1;

	unsigned int plain:1;

	/* XXX: Everything past this comment is specialy handled by compare_opt() */
	unsigned char *framename;

	/* The position of the window. */
	/* This is not compared at all since it doesn't make any difference
	 * what position the document will fit into a frameset or so. */
	int xp, yp;

	/* Active link coloring */
	/* This is mostly here to make use of this option cache so link
	 * drawing is faster. --jonas */
	unsigned int color_active_link:1;
	unsigned int underline_active_link:1;
	color_t active_link_fg;
	color_t active_link_bg;
};

extern struct document_options *d_opt;

/* Fills the structure with values from the option system. */
void init_document_options(struct document_options *doo);

/* Copies the values of one struct @from to the other @to.
 * Note that the framename is dynamically allocated. */
void copy_opt(struct document_options *to, struct document_options *from);

/* Compares comparable values from the two structures according to
 * the comparable members described in the struct definition. */
int compare_opt(struct document_options *o1, struct document_options *o2);

#endif
