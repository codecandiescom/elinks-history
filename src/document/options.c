/* Document options/setup workshop */
/* $Id: options.c,v 1.2 2002/05/08 13:55:02 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "document/options.h"
#include "document/html/colors.h"


struct document_options *d_opt;

int compare_opt(struct document_options *o1, struct document_options *o2)
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

void copy_opt(struct document_options *o1, struct document_options *o2)
{
	memcpy(o1, o2, sizeof(struct document_options));
	o1->framename = stracpy(o2->framename);
}
