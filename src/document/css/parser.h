/* $Id: parser.h,v 1.6 2004/01/18 14:23:03 pasky Exp $ */

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

#endif
