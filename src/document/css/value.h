/* $Id: value.h,v 1.2 2004/01/17 15:46:31 pasky Exp $ */

#ifndef EL__DOCUMENT_CSS_VALUE_H
#define EL__DOCUMENT_CSS_VALUE_H

#include "document/css/css.h"

/* This is interface for the value parser. It is intended to be used only
 * internally inside of the CSS engine. */

/* This function takes a value of a specified type from the given string and
 * converts it to a reasonable {struct css_property}-ready form. */
/* It returns positive integer upon success, zero upon parse error, and moves
 * the string pointer to the byte after the value end. */
int css_parse_value(enum css_decl_valtype valtype, union css_decl_value *value,
		    unsigned char **string);

#endif
