/* $Id: rectangle.h,v 1.3 2002/12/31 00:30:58 pasky Exp $ */

#ifndef EL__USIVE_LAYOUTER_RECTANGLE_H
#define EL__USIVE_LAYOUTER_RECTANGLE_H

/* This file describes the ELusive rectangles tree structures and utility
 * tools. The rectangles tree represents rectangles grouped in larger
 * rectangles grouped in even larger rectangles. Each rectangle has a lot of
 * various attributes, which may or may not include position and size, in one
 * or two dimensions.
 *
 * Usually, each rectangle is associated with one element (syntax tree node).
 * It composes data of several syntax tree, one taking as the "base" one and
 * looking up stuff in the other ones (a particular mechanism is per-layouter
 * specific). */

#include "elusive/parser/syntree.h"

#include "util/lists.h"


struct layout_rectangle_text {
	unsigned char *str;
	int len;
};

enum layout_rectangle_data {
	RECT_NONE, /* data should be ignored; used for super-rectangles and
		    * spacers */
	RECT_TEXT, /* struct layout_rectangle_text */
	/* TODO: RECT_IMAGE, RECT_HR, RECT_OBJECT, RECT_IFRAME, ... */
};

struct layout_rectangle {
	/* Tree position stuff. First, for easy lists handling, position in the
	 * list of leafs of the parent rectangle. */

	struct layout_rectangle *next;
	struct layout_rectangle *prev;

	struct layout_rectangle *root;
	struct list_head leafs; /* -> struct layout_rectangle */

	/* Points to the corresponding node of the base syntax tree. Note that
	 * it's a perfectly valid situation that this is NULL! Theoretically,
	 * there may be no syntax tree at all and the layouter is using a
	 * different underlying mechanism (ie. PDF layouter or some rectangles
	 * in the output of the syntree layouter). */

	struct syntree_node *syntree_node;

	/* Attributes of the rectangle. You know - colors, alignment and so on.
	 * If the attribute is not here, you should also look to the
	 * syntree_node if the attribute is not there (if syntree_node is not
	 * NULL, obviously). We try to save memory, you know. */
	/* For missing attributes, we will just ascend to the parent and look
	 * there, and we will keep doing this until we find something. */
	/* See parser/attrib.h for a description of the struct attribute. The
	 * approach of list of individual attribute structures may seem
	 * slightly ineffective - I decided on this in order to extend the
	 * flexibility considerably; it's expected that renderers will wrap the
	 * struct syntree_node in some their struct where they will cache the
	 * values taken from this list which they do support, so the overhead
	 * should be minimal then (only memory and one move-around; the memory
	 * thing can be a little painful, but see above). */

	struct list_head attrs; /* -> struct attribute */

	/* This is a data container of the type - it depends on the type
	 * modifier. See the enum declaration for information about possible
	 * types of the data. */
	/* If data is non-NULL, it's free()d when destroying the rectangle. */

	void *data;
	enum layout_rectangle_data data_type;
};


/* Initializes a node structure. Returns NULL upon allocation failure. */
struct layout_rectangle *
init_layout_rectangle();

/* Releases the rectangle structure and all its attributes and leafs. It
 * doesn't release the corresponding syntax tree! */
void
done_layout_rectangle(struct layout_rectangle *rect);

/* Returns value string of an attribute with this name. NULL means there's no
 * such attribute set, otherwise a pointer is returned that points to
 * dynamically allocated memory (which you have to free after doing what you
 * needed to, yes). */
/* This automatically looks up the syntax tree attributes list if needed. */
unsigned char *
get_rect_attrib(struct layout_rectangle *rect, unsigned char *name);

#endif
