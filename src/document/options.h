/* $Id: options.h,v 1.42 2004/01/19 19:58:11 jonas Exp $ */

#ifndef EL__DOCUMENT_OPTIONS_H
#define EL__DOCUMENT_OPTIONS_H

#include "terminal/color.h"
#include "util/color.h"

/* This mostly acts as a option cache so rendering will be faster. However it
 * is also used to validate and invalidate documents in the format cache as to
 * whether they satisfy the current state of the document options. */
struct document_options {
	enum color_mode color_mode;
	int cp, assume_cp, hard_assume;
	int margin;
	int num_links_key;
	int use_document_colors;
	int meta_link_display;
	int default_form_input_size;

	/* The default (fallback) colors. */
	color_t default_fg;
	color_t default_bg;
	color_t default_link;
	color_t default_vlink;

	/* Color model/optimizations */
	enum color_flags color_flags;

	/* XXX: Keep boolean options grouped to save padding */
	/* CSS stuff */
	unsigned int css_enable:1;

	/* HTML stuff */
	unsigned int tables:1;
	unsigned int table_order:1;
	unsigned int table_expand_cols:1;
	unsigned int frames:1;
	unsigned int images:1;

	unsigned int display_subs:1;
	unsigned int display_sups:1;
	unsigned int underline_links:1;

	unsigned int wrap_nbsp:1;

	/* Plain rendering stuff */
	unsigned int plain_display_links:1;
	unsigned int plain_compress_empty_lines:1;

	/* Link navigation */
	unsigned int num_links_display:1;
	unsigned int use_tabindex:1;

	unsigned int plain:1;

	/* XXX: Everything past this comment is specialy handled by compare_opt() */
	unsigned char *framename;

	/* The position of the window. */
	/* This is not compared at all since it doesn't make any difference
	 * what position the document will fit into a frameset or so. */
	int x, y;

	/* The width of the window. */
	/* This controls how wide tables can be rendered and so on. It is thus
	 * also to blame for the extra memory consumption when resizing because
	 * all documents has to be rerendered. */
	/* Placed here because we only need to compare it if not @plain. */
	int width;

	/* The height of the window */
	/* It is placed here because only documents containing textarea or
	 * frames uses it and we only compare it if @needs_height is set. */
	int height;
	unsigned int needs_height:1;
	unsigned int needs_width:1;

	/* Active link coloring */
	/* This is mostly here to make use of this option cache so link
	 * drawing is faster. --jonas */
	unsigned int color_active_link:1;
	unsigned int underline_active_link:1;
	unsigned int bold_active_link:1;
	unsigned int invert_active_link:1;
	color_t active_link_fg;
	color_t active_link_bg;
};

extern struct document_options *global_doc_opts;

/* Fills the structure with values from the option system. */
void init_document_options(struct document_options *doo);

/* Copies the values of one struct @from to the other @to.
 * Note that the framename is dynamically allocated. */
void copy_opt(struct document_options *to, struct document_options *from);

/* Compares comparable values from the two structures according to
 * the comparable members described in the struct definition. */
int compare_opt(struct document_options *o1, struct document_options *o2);

#define doc_colors_defaults(o) ((o)->use_document_colors == 0)
#define doc_colors_no_bg(o) ((o)->use_document_colors == 1)
#define doc_colors_and_bg(o) ((o)->use_document_colors == 2)

#endif
