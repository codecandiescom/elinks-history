/* $Id: align.h,v 1.4 2002/12/07 15:28:36 pasky Exp $ */

#ifndef EL__BFU_ALIGN_H
#define EL__BFU_ALIGN_H

/* This enum is pretty ugly, yes ;). */
enum format_align {
	AL_LEFT,
	AL_CENTER,
	AL_RIGHT,
	AL_BLOCK,
	AL_NO,

	AL_MASK = 0x7f,

	/* XXX: DIRTY! For backward compatibility with old menu code: */
	AL_EXTD_TEXT = 0x80,
};

#include "config/options.h"
#include "document/html/colors.h"
#include "lowlevel/terminal.h"
#include "util/string.h"

#define COL(x)	((x)*0x100)

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
	opt = straconcat("ui.colors.", colors ? "color" : "mono", ".", color_class, NULL);
	if (!opt) return 0;
	opt_tree = get_opt_rec(&root_options, opt);
	mem_free(opt);
	if (!opt_tree) return 0;

	if (!nofg) fg = find_nearest_color(get_opt_ptr_tree(opt_tree, "text"), 16);
	bg = find_nearest_color(get_opt_ptr_tree(opt_tree, "background"), 8);
	/* XXX: Call fg_color() ? --pasky */

	return COL(((fg&0x08)<<3)|(bg<<3)|(fg&0x07));
}

#endif
