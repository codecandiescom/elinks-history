/* $Id: parser.h,v 1.10 2004/01/22 18:43:22 jonas Exp $ */

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
