/* Syntax tree utility tools */
/* $Id: syntree.c,v 1.1 2002/12/26 02:29:46 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/attrib.h"
#include "elusive/parser/syntree.h"


/* TODO: Possibly ascend to the root. */
unsigned char *
get_syntree_attrib(struct syntree_node *node, unsigned char *name)
{
	struct attribute *attr = get_attrib(node->attrs, name);
	unsigned char *value;

	if (!attr) return NULL;

	value = mem_alloc(attr->svallen + 1);
	strncpy(value, attr->sval, attr->svallen);
	value[attr->svallen] = 0;

	return value;
}
