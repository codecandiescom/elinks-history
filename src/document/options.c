/* Document options/setup workshop */
/* $Id: options.c,v 1.27 2003/10/09 09:49:38 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "document/options.h"
#include "util/color.h"
#include "util/string.h"


/* Some compilers, like SunOS4 cc, don't have offsetof in <stddef.h>.  */
#ifndef offsetof
#define offsetof(type, ident) ((size_t) &(((type *) 0)->ident))
#endif

struct document_options *d_opt;

void
init_document_options(struct document_options *doo)
{
	memset(doo, 0, sizeof(struct document_options));

	doo->assume_cp = get_opt_int("document.codepage.assume");
	doo->hard_assume = get_opt_int("document.codepage.force_assumed");

	doo->use_document_colours = get_opt_int("document.colors.use_document_colors");
	doo->margin = get_opt_int("document.browse.margin_width");
	doo->num_links_key = get_opt_int("document.browse.links.number_keys_select_link");
	doo->meta_link_display = get_opt_int("document.html.link_display");

	/* Default colors. */
	doo->default_fg = get_opt_color("document.colors.text");
	doo->default_bg = get_opt_color("document.colors.background");
	doo->default_link = get_opt_color("document.colors.link");
	doo->default_vlink = get_opt_color("document.colors.vlink");

	/* Boolean options. */
	doo->num_links_display = get_opt_bool("document.browse.links.numbering");
	doo->use_tabindex = get_opt_bool("document.browse.links.use_tabindex");
	doo->allow_dark_on_black = get_opt_bool("document.colors.allow_dark_on_black");
	doo->table_order = get_opt_bool("document.browse.table_move_order");
	doo->tables = get_opt_bool("document.html.display_tables");
	doo->frames = get_opt_bool("document.html.display_frames");
	doo->images = get_opt_bool("document.browse.images.show_as_links");
	doo->display_subs = get_opt_bool("document.html.display_subs");
	doo->display_sups = get_opt_bool("document.html.display_sups");

	doo->framename = "";
}

int
compare_opt(struct document_options *o1, struct document_options *o2)
{
	return memcmp(o1, o2, offsetof(struct document_options, framename))
		|| strcasecmp(o1->framename, o2->framename);
}

inline void
copy_opt(struct document_options *o1, struct document_options *o2)
{
	memcpy(o1, o2, sizeof(struct document_options));
	o1->framename = stracpy(o2->framename);
}
