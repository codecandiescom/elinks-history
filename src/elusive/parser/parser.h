/* $Id: parser.h,v 1.1 2002/12/25 00:30:11 pasky Exp $ */

#ifndef EL__USIVE_PARSER_PARSER_H
#define EL__USIVE_PARSER_PARSER_H

/* This is a universal parser interface, as exposed to ELinks (in the ideal
 * case). */

#include "elusive/parser/syntree.h"

enum parser_backend_type {
	PARSER_DTD,
	PARSER_CSS,
	PARSER_HTML,
};

/* The main parser entry point. If root is NULL, it'll take str as the
 * start of the document, otherwise it will take root as a state and it
 * will take str as string immediatelly following the previous str supplied
 * to elusive_parser(). */
void
elusive_parser(enum parser_backend_type parser, struct syntree_node **root,
		unsigned char *str, int len);

#endif
