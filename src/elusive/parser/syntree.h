/* $Id: syntree.h,v 1.3 2002/12/26 02:29:46 pasky Exp $ */

#ifndef EL__USIVE_PARSER_SYNTREE_H
#define EL__USIVE_PARSER_SYNTREE_H

/* Note that ELusive is optimized to speed, not to memory. If we would want to
 * save memory, we would re-parse the tag everytime we would encounter it,
 * saving space required for the pointer arrays, decoded values etc. Maybe this
 * would be an interesting option, but that depends on the complexity of such a
 * change. */

/* This file describes the ELusive syntax tree structures and utility tools.
 * The syntax tree is not a classical tree, but more like an... inheritance
 * tree. That is, each syntax tree node describes an _element_ - not only the
 * _tag_, but also content of the area enclosed by starting and ending tag.
 *
 * There's so-called root element, which is associated to ie. <html> or <xml>
 * element if found, and generically supplied otherwise. It usually contains
 * head and body elements as leafs and so on. */

#include "util/lists.h"

enum syntree_node_special {
	NODE_SPEC_NONE,
	NODE_SPEC_TEXT,
};

struct syntree_node {
	/* Tree position stuff. First, for easy lists handling, position in the
	 * list of leafs of the parent node. */

	struct syntree_node *next;
	struct syntree_node *prev;

	struct syntree_node *root;
	struct list_head leafs; /* -> struct syntree_node */

	/* Credentials of this node. Who are we and what do we live for? */
	/* In fact, ideally we shouldn't need this ie. for HTML, since
	 * everything relevant for us will be stored as attributes. Sure
	 * there are some magic elements - these are described below. */
	/* Note that name can be NULL - that's the case for the text special,
	 * which is a terminal token - the text itself. It can be then taken
	 * from src/srclen. */

	unsigned char *name;
	int namelen;

	/* Sometimes it'd be too expensive to describe something generically.
	 * Imagine tables or frames or some head stuff or even forms or so. */
	enum syntree_node_special special;
	void *special_data; /* Note that this is mem_free()d if non-NULL. */

	/* Attributes of the node. You know - colors, alignment and so on.
	 * For missing attributes, we will just ascend to the parent and look
	 * there, and we will keep doing this until we will find something.
	 * At the root node, we will default to something. */
	/* See attrib.h for a description of the struct attribute. The approach
	 * of list of individual attribute structures may seem slightly
	 * ineffective - I decided on this in order to extend the flexibility
	 * considerably; it's expected that renderers will wrap the struct
	 * syntree_node in some their struct where they will cache the values
	 * taken from this list which they do support, so the overhead should
	 * be minimal then (only memory and one move-around; the memory thing
	 * can be a little painful, but see above). */

	struct list_head attrs; /* -> struct attribute */

	/* Pointer to the HTML source. Srclen determines the length of the
	 * whole element, including the opening/closing tags. */

	unsigned char *src;
	int srclen;
};

/* Returns value string of an attribute with this name. NULL means there's no
 * such attribute set, otherwise a pointer is returned that points to
 * dynamically allocated memory (which you have to free after doing what you
 * needed to, yes). */
unsigned char *
get_syntree_attrib(struct syntree_node *node, unsigned char *name);

#endif
