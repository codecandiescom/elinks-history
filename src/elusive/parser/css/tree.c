/* CSS tree utility tools */
/* $Id: tree.c,v 1.2 2003/06/05 14:38:17 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/css/tree.h"
#include "elusive/parser/css/test.h"
#include "elusive/parser/parser.h"
#include "elusive/parser/property.h"
#include "util/memory.h"
#include "util/string.h"

struct stylesheet *
init_stylesheet(void)
{
	struct stylesheet *stylesheet;
	struct css_hash_item *universal;

	stylesheet = mem_calloc(1, sizeof(struct stylesheet));
	if (!stylesheet) return NULL;

	universal = mem_alloc(sizeof(struct css_hash_item));
	if (universal) {
		universal->name = "*";
		universal->namelen = 1;
		init_list(universal->nodes);
		stylesheet->universal = universal;

		stylesheet->hash = init_hash(10, &strhash);
	}

	if (universal && stylesheet->hash) {
		if (add_hash_item(stylesheet->hash, "*", 1, universal)) {
			return stylesheet;
		}
	}

	/* Allocation failure handling from here and down */
	if (stylesheet->hash) free_hash(stylesheet->hash);
	if (universal) mem_free(universal);
	mem_free(stylesheet);
	return NULL;
}

void
done_stylesheet(struct stylesheet *stylesheet)
{
	struct hash_item *item;
	int i;

	foreach_hash_item(item, *stylesheet->hash, i)
		if (item->value) {
			struct css_hash_item *element = item->value;
			struct css_node *node;

			printf("Deleting hash item ");
			print_token("", element->name, element->namelen);

			foreach (node, element->nodes)
				done_css_node(node);

			mem_free(element);
		}

	free_hash(stylesheet->hash);
	mem_free(stylesheet);
}

struct css_node *
init_css_node(void)
{
	struct css_node *node;

	node = mem_calloc(1, sizeof(struct css_node));
	if (!node) return NULL;

	init_list(node->leafs);
	init_list(node->properties);
	init_list(node->attributes);

	return node;
}

struct css_node *
spawn_css_node(struct parser_state *state, unsigned char *name, int namelen)
{
	struct css_node *node = init_css_node();

	if (!node) return NULL;

	/* If the root is set it means the node should be added as a leaf to the
	 * state->current. */
	if (state->root) state->root = state->current;

	node->str = name;
	node->strlen = namelen;
	state->current = node;
	print_css_node("Spawned node", node);
	return node;
}

struct css_node *
add_css_node(struct parser_state *state)
{
	struct css_node *node = state->current;

#ifdef CSS_DEBUG
	if (!node) internal("Trying to add non-existing css node");
#endif

	/* Do we have to set up the root or just descend ? */
	if (!state->root) {
		struct stylesheet *stylesheet = state->real_root;
		struct hash_item *item;
		struct css_hash_item *element;

		/* First check if the element is already checked in */
		item = get_hash_item(stylesheet->hash, name, namelen);
		if (!item) {
			element = mem_alloc(sizeof(struct css_hash_item));
			if (!element) {
				done_css_node(node);
				return NULL;
			}
			printf("Adding at root in new hash entry\n");

			element->name = name;
			element->namelen = namelen;
			init_list(element->nodes);

			if (!add_hash_item(stylesheet->hash, name, namelen, element)) {
				done_css_node(node);
				mem_free(element);
				return NULL;
			}
		} else {
			element = item->value;

			printf("Adding at root in old hash entry\n");
		}

		state->root = node;
	} else {
		struct css_node *root root = state->root;

		printf("Adding to leaf of ");
		print_token("", root->str, root->strlen);
		state->root = state->current;
	}
}

void
done_css_node(struct css_node *node)
{
	struct css_node *leaf = node->leafs.next;
	struct property *property = node->properties.next;
	struct attribute_matching *attribute = node->attributes.next;

	if (!node->str) return;
	print_css_node("Deleting ", node);

	while ((struct list_head *) leaf != &node->leafs) {
		struct css_node *leaf_next = leaf->next;

		done_css_node(leaf);
		leaf = leaf_next;
	}

	while ((struct list_head *) attribute != &node->attributes) {
		struct css_attr *attr_next = attribute->next;

		mem_free(attribute);
		attribute = attr_next;
	}

	while ((struct list_head *) property != &node->properties) {
		struct property *property_next = property->next;

		mem_free(property);
		property = property_next;
	}

	if (node->src)
		mem_free(node->str);

	if (node->next) del_from_list(node);

	mem_free(node);
}

struct css_attr_match *
add_css_attr(struct parser_state *state, unsigned char *name,
	     int namelen, enum css_attr_match_type type)
{
	struct css_node *node = state->current;
	struct css_attr_match *attr = mem_calloc(1, sizeof(struct css_attr));
	struct css_attr_match *other;

#ifdef CSS_DEBUG
	if (!node) internal("Trying to add attribute to nonexisting css node");
#endif

	if (!attr) return NULL;

	attr->type = type;

	/* Add in alphabetical order */
	/* Either add right after the attribute name is 'smaller' (comes
	 * before) than the 'other' or add last. */
	foreach (other, node->attributes)
		if ((strncasecmp(name, other->name, namelen) < 0))
			break;

	/* 'other' will be equal to the list head */
	add_at_pos(other->prev, attr);
	return attr;
}

unsigned char *
get_css_property(struct css_node *node, unsigned char *name)
{
	struct property *property = get_property(&node->properties, name);

	if (!property)
		return NULL;

	return memacpy(property->value, property->valuelen);
}
