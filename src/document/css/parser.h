/* $Id: parser.h,v 1.8 2004/01/19 19:01:42 jonas Exp $ */

#ifndef EL__DOCUMENT_CSS_PARSER_H
#define EL__DOCUMENT_CSS_PARSER_H

#include "util/lists.h"

/* This is interface for the value parser. It is intended to be used only
 * internally inside of the CSS engine. */

/* This function takes a semicolon separated list of declarations from the
 * given string, parses them to atoms, and chains the newly created {struct
 * css_property}es to the specified list. The function returns positive value
 * in case it recognized a property in the given string, or zero in case of an
 * error. */
void css_parse_properties(struct list_head *props, unsigned char *string);


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

void css_parse_stylesheet(struct css_stylesheet *css, unsigned char *string);
void done_css_stylesheet(struct css_stylesheet *css);
struct css_selector *get_css_selector(struct css_stylesheet *css, unsigned char *name, int namelen);

#endif
