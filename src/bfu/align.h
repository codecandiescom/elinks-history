/* $Id: align.h,v 1.15 2003/06/18 01:11:44 jonas Exp $ */

#ifndef EL__BFU_ALIGN_H
#define EL__BFU_ALIGN_H

#include "config/options.h"
#include "document/html/colors.h"
#include "terminal/terminal.h"
#include "util/string.h"

/* This enum is pretty ugly, yes ;). */
enum format_align {
	AL_LEFT,
	AL_CENTER,
	AL_RIGHT,
	AL_BLOCK,
	AL_NONE,
};

#define COL(x)	((x)<<8)

/* FIXME: A bit ... */
static inline int
get_bfu_color(struct terminal *term, unsigned char *color_class)
{
	struct option *opt_tree;
	int colors;
	unsigned char *opt;
	int fg = 0, bg = 0;
	int nofg = (color_class[0] == '=');

	if (nofg) color_class++;

	if (!term) return 0;
	opt_tree = term->spec;

	colors = get_opt_bool_tree(opt_tree, "colors");
	opt = straconcat((unsigned char *) "ui.colors.",
			 colors ? "color" : "mono", ".", color_class, NULL);
	if (!opt) return 0;
	opt_tree = get_opt_rec(&root_options, opt);
	mem_free(opt);
	if (!opt_tree) return 0;

	if (!nofg)
		fg = find_nearest_color(get_opt_ptr_tree(opt_tree, "text"), 16);

	bg = find_nearest_color(get_opt_ptr_tree(opt_tree, "background"), 8);
	/* XXX: Call fg_color() ? --pasky */

	return COL(((fg&0x08)<<3)|(bg<<3)|(fg&0x07));
}

#endif
