/* Syntax tree utility tools */
/* $Id: syntree.c,v 1.11 2003/01/17 22:04:41 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/parser.h"
#include "elusive/parser/property.h"
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
	init_list(node->properties);

	return node;
}

void
done_syntree_node(struct syntree_node *node)
{
	struct syntree_node *leaf = node->leafs.next;
	struct property *property = node->properties.next;

	while ((struct list_head *) leaf != &node->leafs) {
		struct syntree_node *leaf_next = leaf->next;

		done_syntree_node(leaf);
		leaf = leaf_next;
	}

	while ((struct list_head *) property != &node->properties) {
		struct property *property_next = property->next;

		/* TODO: Implement free function in property.c. */
		mem_free(property);
		property = property_next;
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
get_syntree_property(struct syntree_node *node, unsigned char *name)
{
	struct property *property = get_property(node->properties, name);

	if (!property) return NULL;

	return memacpy(property->value, property->valuelen);
}
