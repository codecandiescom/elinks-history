/* $Id: parser.h,v 1.2 2002/12/26 02:46:10 pasky Exp $ */

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

struct parser_backend {
	void (*parse)(struct syntree_node **, unsigned char **, int *);
};

/* The main parser entry point. If root is NULL, it'll take str as the
 * start of the document, otherwise it will take root as a state and it
 * will take str as string immediatelly following the previous str supplied
 * to elusive_parser(). The str will usually end up pointing at the end of
 * the string, but it can point at few chars before that point, if it's
 * needed for an intermediate state (ie. while parsing tag content). */
void
elusive_parser(enum parser_backend_type parser, struct syntree_node **root,
		unsigned char **str, int *len);

#endif
