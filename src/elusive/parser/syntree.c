/* Syntax tree utility tools */
/* $Id: syntree.c,v 1.4 2002/12/27 01:19:06 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/attrib.h"
#include "elusive/parser/syntree.h"


struct syntree_node *
init_syntree_node()
{
	struct syntree_node *node;

	node = mem_calloc(1, sizeof(struct syntree_node));
	if (!node) return NULL;

	return node;
}

/* TODO: Possibly ascend to the root. */
unsigned char *
get_syntree_attrib(struct syntree_node *node, unsigned char *name)
{
	struct attribute *attr = get_attrib(node->attrs, name);
	unsigned char *value;

	if (!attr) return NULL;

	value = mem_alloc(attr->valuelen + 1);
	strncpy(value, attr->value, attr->valuelen);
	value[attr->valuelen] = 0;

	return value;
}
