/* $Id: align.h,v 1.19 2003/07/06 23:17:33 pasky Exp $ */

#ifndef EL__BFU_ALIGN_H
#define EL__BFU_ALIGN_H

enum format_align {
	AL_LEFT,
	AL_CENTER,
	AL_RIGHT,
	AL_BLOCK,
	AL_NONE,
};

/* The following really does not belong here, but it was so convenient to stick
 * it here, that... ;-))) --pasky */

#include "config/options.h"
#include "document/html/colors.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/string.h"

#define COL(x)	((x)<<8)

/* FIXME: A bit ... */
static inline int
get_bfu_color(struct terminal *term, unsigned char *color_class)
{
	struct option *opt_tree;
	unsigned char *opt;
	int fg;
	int bg;
	int nofg;

	assert(color_class && *color_class);
	if_assert_failed return 0;

	if (!term) return 0;

	opt_tree = term->spec;

	nofg = (color_class[0] == '=');
	if (nofg) color_class++;

	/* Here we use malloc()+strcpy()+strcat() instead of straconcat() since
	 * performance matters. --Zas */
	opt = fmem_alloc(strlen(color_class) + 16 /* strlen("ui.colors.color.") */ + 1);
	if (!opt) return 0;
	if (get_opt_bool_tree(opt_tree, "colors"))
		strcpy(opt, "ui.colors.color.");
	else
		strcpy(opt, "ui.colors.mono.");
	strcat(opt, color_class);

	opt_tree = get_opt_rec(&root_options, opt);
	fmem_free(opt);
	if (!opt_tree) return 0;

	bg = find_nearest_color(get_opt_ptr_tree(opt_tree, "background"), 8);

	/* XXX: Call fg_color() ? --pasky */

	if (nofg) return COL(bg<<3);
	fg = find_nearest_color(get_opt_ptr_tree(opt_tree, "text"), 16);
	return COL(((fg&0x08)<<3)|(bg<<3)|(fg&0x07));
}

#endif
