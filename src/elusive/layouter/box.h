/* $Id: box.h,v 1.2 2003/01/17 22:04:41 pasky Exp $ */

#ifndef EL__USIVE_LAYOUTER_BOX_H
#define EL__USIVE_LAYOUTER_BOX_H

/* This file describes the ELusive boxes tree structures and utility tools. The
 * boxes tree represents boxes grouped in larger boxes grouped in even larger
 * boxes. Each box has a lot of various properties, which may or may not
 * include position and size, in one or two dimensions.
 *
 * Usually, each box is associated with one element (syntax tree node).  It
 * composes data of several syntax tree, one taking as the "base" one and
 * looking up stuff in the other ones (a particular mechanism is per-layouter
 * specific). */

#include "elusive/parser/syntree.h"

#include "util/lists.h"


struct layout_box_text {
	unsigned char *str;
	int len;
};

enum layout_box_data {
	RECT_NONE, /* data should be ignored; used for super-boxes and
		    * spacers */
	RECT_TEXT, /* struct layout_box_text */
	/* TODO: RECT_IMAGE, RECT_HR, RECT_OBJECT, RECT_IFRAME, ... */
};

struct layout_box {
	/* Tree position stuff. First, for easy lists handling, position in the
	 * list of leafs of the parent box. */

	struct layout_box *next;
	struct layout_box *prev;

	struct layout_box *root;
	struct list_head leafs; /* -> struct layout_box */

	/* Points to the corresponding node of the base syntax tree. Note that
	 * it's a perfectly valid situation that this is NULL! Theoretically,
	 * there may be no syntax tree at all and the layouter is using a
	 * different underlying mechanism (ie. PDF layouter or some boxes
	 * in the output of the syntree layouter). */

	struct syntree_node *syntree_node;

	/* Properties of the box. You know - colors, alignment and so on.
	 * If the property is not here, you should also look to the
	 * syntree_node if the property is not there (if syntree_node is not
	 * NULL, obviously). We try to save memory, you know. */
	/* For missing properties, we will just ascend to the parent and look
	 * there, and we will keep doing this until we find something. */
	/* See parser/property.h for a description of the struct property. The
	 * approach of list of individual properte structures may seem
	 * slightly ineffective - I decided on this in order to extend the
	 * flexibility considerably; it's expected that renderers will wrap the
	 * struct syntree_node in some their struct where they will cache the
	 * values taken from this list which they do support, so the overhead
	 * should be minimal then (only memory and one move-around; the memory
	 * thing can be a little painful, but see above). */

	struct list_head properties; /* -> struct property */

	/* This is a data container of the type - it depends on the type
	 * modifier. See the enum declaration for information about possible
	 * types of the data. */
	/* If data is non-NULL, it's free()d when destroying the box. */

	void *data;
	enum layout_box_data data_type;
};


/* Initializes a node structure. Returns NULL upon allocation failure. */
struct layout_box *
init_layout_box();

/* Releases the box structure and all its properties and leafs. It
 * doesn't release the corresponding syntax tree! */
void
done_layout_box(struct layout_box *box);

/* Returns value string of an property with this name. NULL means there's no
 * such property set, otherwise a pointer is returned that points to
 * dynamically allocated memory (which you have to free after doing what you
 * needed to, yes). */
/* This automatically looks up the syntax tree properties list if needed. */
unsigned char *
get_box_property(struct layout_box *box, unsigned char *name);

#endif
