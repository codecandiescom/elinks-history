/* Layout box utility tools */
/* $Id: box.c,v 1.1 2003/01/17 21:48:34 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/layouter/box.h"
#include "elusive/parser/attrib.h"
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
	init_list(box->attrs);

	return box;
}

void
done_layout_box(struct layout_box *box)
{
	struct layout_box *leaf = box->leafs.next;
	struct attribute *attrib = box->attrs.next;

	while ((struct list_head *) leaf != &box->leafs) {
		struct layout_box *leaf_next = leaf->next;

		done_layout_box(leaf);
		leaf = leaf_next;
	}

	while ((struct list_head *) attrib != &box->attrs) {
		struct attribute *attrib_next = attrib->next;

		/* TODO: Implement free function in attrib.c. */
		mem_free(attrib);
		attrib = attrib_next;
	}

	if (box->data)
		mem_free(box->data);

	if (box->next) del_from_list(box);
	mem_free(box);
}


/* TODO: Possibly ascend to the root. */
unsigned char *
get_box_attrib(struct layout_box *box, unsigned char *name)
{
	struct attribute *attr = get_attrib(box->attrs, name);

	/* XXX: When we'll be ascending to root, we won't want to let
	 * get_syntree_attrib() ascend to root, will we? --pasky */
	if (!attr) {
		return box->syntree_node ? get_syntree_attrib(box->syntree_node, name)
					  : NULL;
	}

	return memacpy(attr->value, attr->valuelen);
}
