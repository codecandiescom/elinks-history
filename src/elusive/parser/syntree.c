/* Syntax tree utility tools */
/* $Id: syntree.c,v 1.10 2003/01/01 20:03:07 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/attrib.h"
#include "elusive/parser/parser.h"
#include "elusive/parser/syntree.h"
#include "util/memory.h"
#include "util/string.h"


struct syntree_node *
init_syntree_node()
{
	struct syntree_node *node;

	node = mem_calloc(1, sizeof(struct syntree_node));
	if (!node) return NULL;

	init_list(node->leafs);
	init_list(node->attrs);

	return node;
}

void
done_syntree_node(struct syntree_node *node)
{
	struct syntree_node *leaf = node->leafs.next;
	struct attribute *attrib = node->attrs.next;

	while ((struct list_head *) leaf != &node->leafs) {
		struct syntree_node *leaf_next = leaf->next;

		done_syntree_node(leaf);
		leaf = leaf_next;
	}

	while ((struct list_head *) attrib != &node->attrs) {
		struct attribute *attrib_next = attrib->next;

		/* TODO: Implement free function in attrib.c. */
		mem_free(attrib);
		attrib = attrib_next;
	}

	if (node->src)
		mem_free(node->str);
	if (node->special_data)
		mem_free(node->special_data);

	if (node->next) del_from_list(node);
	mem_free(node);
}


struct syntree_node *
spawn_syntree_node(struct parser_state *state)
{
	struct syntree_node *node = init_syntree_node();

	if (!node) return NULL;

	node->root = state->root;
	if (state->root != state->current) {
		add_at_pos(state->current, node);
	} else {
		/* We've spawned non-leaf node right before. So we will fit
		 * under it (not along it) nicely. */
		add_to_list(state->root->leafs, node);
	}
	state->current = node;

	return node;
}


/* TODO: Possibly ascend to the root. */
unsigned char *
get_syntree_attrib(struct syntree_node *node, unsigned char *name)
{
	struct attribute *attr = get_attrib(node->attrs, name);

	if (!attr) return NULL;

	return memacpy(attr->value, attr->valuelen);
}
