/* Layout rectangle utility tools */
/* $Id: rectangle.c,v 1.2 2002/12/30 18:04:50 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/layouter/rectangle.h"
#include "elusive/parser/attrib.h"
#include "elusive/parser/syntree.h"
#include "util/memory.h"


struct layout_rectangle *
init_layout_rectangle()
{
	struct layout_rectangle *rect;

	rect = mem_calloc(1, sizeof(struct layout_rectangle));
	if (!rect) return NULL;

	init_list(rect->leafs);
	init_list(rect->attrs);

	return node;
}

void
done_layout_rectangle(struct layout_rectangle *rect)
{
	struct layout_rectangle *leaf = rect->leafs.next;
	struct attribute *attrib = rect->attrs.next;

	while ((struct list_head *) leaf != &rect->leafs) {
		struct layout_rectangle *leaf_next = leaf->next;

		done_layout_rectangle(leaf);
		leaf = leaf_next;
	}

	while ((struct list_head *) attrib != &rect->attrs) {
		struct attribute *attrib_next = attrib->next;

		/* TODO: Implement free function in attrib.c. */
		mem_free(attrib);
		attrib = attrib_next;
	}

	if (rect->data)
		mem_free(rect->data);

	if (rect->next) del_from_list(rect);
	mem_free(rect);
}


/* TODO: Possibly ascend to the root. */
unsigned char *
get_rect_attrib(struct layout_rectangle *rect, unsigned char *name)
{
	struct attribute *attr = get_attrib(rect->attrs, name);
	unsigned char *value;

	/* XXX: When we'll be ascending to root, we won't want to let
	 * get_syntree_attrib() ascend to root, will we? --pasky */
	if (!attr) {
		return rect->syntree_node ? get_syntree_attrib(rect->syntree_node, name)
					  : NULL;
	}

	value = mem_alloc(attr->valuelen + 1);
	strncpy(value, attr->value, attr->valuelen);
	value[attr->valuelen] = 0;

	return value;
}
