/* Document options/setup workshop */
/* $Id: options.c,v 1.21 2003/09/27 00:32:03 jonas Exp $ */

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
#define offsetof(type,ident) ((size_t)&(((type*)0)->ident))
#endif

struct document_options *d_opt;


void
mk_document_options(struct document_options *doo)
{
	doo->assume_cp = get_opt_int("document.codepage.assume");
	doo->hard_assume = get_opt_int("document.codepage.force_assumed");
	doo->use_document_colours = get_opt_int("document.colors.use_document_colors");
	doo->margin = get_opt_int("document.browse.margin_width");
	doo->num_links_key = get_opt_int("document.browse.links.number_keys_select_link");

	doo->num_links_display = get_opt_bool("document.browse.links.numbering");
	doo->allow_dark_on_black = get_opt_bool("document.colors.allow_dark_on_black");
	doo->table_order = get_opt_bool("document.browse.table_move_order");
	doo->tables = get_opt_bool("document.html.display_tables");
	doo->frames = get_opt_bool("document.html.display_frames");
	doo->images = get_opt_bool("document.browse.images.show_as_links");
	doo->display_subs = get_opt_bool("document.html.display_subs");
	doo->display_sups = get_opt_bool("document.html.display_sups");
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
