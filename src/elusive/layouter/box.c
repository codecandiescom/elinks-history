/* Layout box utility tools */
/* $Id: box.c,v 1.3 2003/01/18 00:36:13 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/layouter/box.h"
#include "elusive/parser/property.h"
#include "elusive/parser/syntree.h"
#include "util/memory.h"
#include "util/string.h"


struct layout_box *
init_layout_box()
{
	struct layout_box *box;

	box = mem_calloc(1, sizeof(struct layout_box));
	if (!box) return NULL;

	init_list(box->leafs);
	init_list(box->properties);

	return box;
}

void
done_layout_box(struct layout_box *box)
{
	struct layout_box *leaf = box->leafs.next;
	struct property *property = box->properties.next;

	while ((struct list_head *) leaf != &box->leafs) {
		struct layout_box *leaf_next = leaf->next;

		done_layout_box(leaf);
		leaf = leaf_next;
	}

	while ((struct list_head *) property != &box->properties) {
		struct property *property_next = property->next;

		/* TODO: Implement free function in property.c. */
		mem_free(property);
		property = property_next;
	}

	if (box->data)
		mem_free(box->data);

	if (box->next) del_from_list(box);
	mem_free(box);
}


/* TODO: Possibly ascend to the root. */
unsigned char *
get_box_property(struct layout_box *box, unsigned char *name)
{
	struct property *property = get_property(&box->properties, name);

	/* XXX: When we'll be ascending to root, we won't want to let
	 * get_syntree_property() ascend to root, will we? --pasky */
	if (!property) {
		return box->syntree_node ? get_syntree_property(box->syntree_node, name)
					  : NULL;
	}

	return memacpy(property->value, property->valuelen);
}
