/* Document options/setup workshop */
/* $Id: options.c,v 1.6 2002/06/21 14:11:05 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "config/options.h"
#include "document/options.h"
#include "document/html/colors.h"
#include "util/string.h"


struct document_options *d_opt;


void
mk_document_options(struct document_options *doo)
{
	doo->assume_cp = get_opt_int("document.codepage.assume");
	doo->hard_assume = get_opt_int("document.codepage.force_assumed");
	doo->use_document_colours = get_opt_int("document.colors.use_document_colors");
	doo->avoid_dark_on_black = get_opt_int("document.colors.allow_dark_on_black");
	doo->tables = get_opt_int("document.html.display_tables");
	doo->frames = get_opt_int("document.html.display_frames");
	doo->images = get_opt_int("document.browse.images.show_as_links");
	doo->margin = get_opt_int("document.browse.margin_width");
	doo->num_links = get_opt_int("document.browse.links.numbering");
	doo->table_order = get_opt_int("document.browse.table_move_order");
}

int
compare_opt(struct document_options *o1, struct document_options *o2)
{
	if (o1->xw == o2->xw &&
	    o1->yw == o2->yw &&
	    o1->xp == o2->xp &&
	    o1->yp == o2->yp &&
	    o1->col == o2->col &&
	    o1->cp == o2->cp &&
	    o1->assume_cp == o2->assume_cp &&
	    o1->hard_assume == o2->hard_assume &&
	    o1->use_document_colours == o2->use_document_colours &&
	    o1->avoid_dark_on_black == o2->avoid_dark_on_black &&
	    o1->tables == o2->tables &&
	    o1->frames == o2->frames &&
	    o1->images == o2->images &&
	    o1->margin == o2->margin &&
	    o1->plain == o2->plain &&
	    !memcmp(&o1->default_fg, &o2->default_fg, sizeof(struct rgb)) &&
	    !memcmp(&o1->default_bg, &o2->default_bg, sizeof(struct rgb)) &&
	    !memcmp(&o1->default_link, &o2->default_link, sizeof(struct rgb)) &&
	    !memcmp(&o1->default_vlink, &o2->default_vlink, sizeof(struct rgb)) &&
	    o1->num_links == o2->num_links &&
	    o1->table_order == o2->table_order &&
	    !strcasecmp(o1->framename, o2->framename)) return 0;
	return 1;
}

void
copy_opt(struct document_options *o1, struct document_options *o2)
{
	memcpy(o1, o2, sizeof(struct document_options));
	o1->framename = stracpy(o2->framename);
}
