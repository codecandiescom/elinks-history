/* $Id: align.h,v 1.22 2003/07/31 16:56:11 jonas Exp $ */

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
static inline unsigned char
get_bfu_color(struct terminal *term, unsigned char *color_class)
{
	struct option *opt;
	int fg;
	int bg;
	int nofg;

	assert(color_class && *color_class);
	if_assert_failed return 0;

	if (!term) return 0;

	nofg = (color_class[0] == '=');
	if (nofg) color_class++;

	opt = get_opt_rec_real(config_options,
			       get_opt_bool_tree(term->spec, "colors")
					? "ui.colors.color"
					: "ui.colors.mono");
	if (!opt) return 0;

	opt = get_opt_rec_real(opt, color_class);
	if (!opt) return 0;

	bg = find_nearest_color(get_opt_ptr_tree(opt, "background"), 8);

	/* XXX: Call fg_color() ? --pasky */

	if (nofg) return (bg<<3);
	fg = find_nearest_color(get_opt_ptr_tree(opt, "text"), 16);
	return (((fg&0x08)<<3)|(bg<<3)|(fg&0x07));
}

#endif
