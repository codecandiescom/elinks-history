/* $Id: parser.h,v 1.11 2004/01/23 23:59:35 jonas Exp $ */

#ifndef EL__DOCUMENT_CSS_PARSER_H
#define EL__DOCUMENT_CSS_PARSER_H

#include "util/lists.h"
struct css_scanner;

/* This is interface for the value parser. It is intended to be used only
 * internally inside of the CSS engine. */

/* This function takes a semicolon separated list of declarations from the
 * given string, parses them to atoms, and chains the newly created {struct
 * css_property}es to the specified list. The function returns positive value
 * in case it recognized a property in the given string, or zero in case of an
 * error. */
void css_parse_properties(struct list_head *props, struct css_scanner *scanner);


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

/* Parses the @string and adds any recognized selectors + properties to the
 * given stylesheet @css. If the selector is already in the stylesheet it
 * properties are added to the that selector. */
void css_parse_stylesheet(struct css_stylesheet *css, unsigned char *string);

/* Releases all the content of the stylesheet. */
void done_css_stylesheet(struct css_stylesheet *css);

/* Looks up the selector with the name @name and length @namelen in the
 * stylesheet @css. */
struct css_selector *
get_css_selector(struct css_stylesheet *css, unsigned char *name, int namelen);

#endif
