/* $Id: stylesheet.h,v 1.1 2004/01/24 02:05:46 jonas Exp $ */

#ifndef EL__DOCUMENT_CSS_STYLESHEET_H
#define EL__DOCUMENT_CSS_STYLESHEET_H

#include "util/lists.h"

/* TODO: We need a memory efficient and fast way to define how properties
 * cascade. What we are interested in is making it fast and easy to find
 * all properties we need.
 *
 *	struct css_cascade {
 *		struct css_cascade *parent;
 *		struct list_head properties;
 *
 *			- Can later be turned into a table to not waste memory:
 *			  struct css_property properties[1];
 *	};
 *
 * And the selector should then only map a document element into this
 * data structure.
 *
 * All the CSS applier has to do is require the css_cascade of the current
 * element and it should nicely inherit any style from parent cascades.
 * Question is in what direction to apply. It should be possible for the user
 * to overwrite any document provided stylesheet using "!important" so we need
 * to keep track in some table what properties was already applied so we only
 * overwrite when we have to. */

/* For now we only handle really ``flat'' stylesheets. No complicated
 * selectors only good clean element ones. */

struct css_selector {
	LIST_HEAD(struct css_selector);

	unsigned char *element;
	struct list_head properties;
};

struct css_stylesheet {
	struct list_head selectors;
};

/* Releases all the content of the stylesheet. */
void done_css_stylesheet(struct css_stylesheet *css);

/* Looks up the selector with the name @name and length @namelen in the
 * stylesheet @css. */
struct css_selector *
get_css_selector(struct css_stylesheet *css, unsigned char *name, int namelen);

#endif
