/* $Id: parser.h,v 1.1 2004/01/17 15:54:42 pasky Exp $ */

#ifndef EL__DOCUMENT_CSS_PARSER_H
#define EL__DOCUMENT_CSS_PARSER_H

struct list_head;

/* This is interface for the value parser. It is intended to be used only
 * internally inside of the CSS engine. */

/* This function takes a declaration from the given string, parses it to atoms,
 * and possibly creates {struct css_property} and chains it up to the specified
 * list. The function returns positive value in case it recognized a property
 * in the given string, or zero in case of an error. */
/* This function is recursive, therefore if you give it a string containing
 * multiple declarations separated by a semicolon, it will call itself for each
 * of the following declarations. Then it returns success in case at least one
 * css_parse_decl() run succeeded. In case of failure, it tries to do an error
 * recovery by simply looking at the nearest semicolon ahead. */
int css_parse_decl(struct list_head *props, unsigned char *string);

#endif
