/* $Id: parser.h,v 1.4 2002/12/27 01:19:06 pasky Exp $ */

#ifndef EL__USIVE_PARSER_PARSER_H
#define EL__USIVE_PARSER_PARSER_H

/* This is a universal parser interface, as exposed to ELinks (in the ideal
 * case). */

#include "elusive/parser/syntree.h"


struct parser_state {
	struct syntree_node *root;
	struct syntree_node *current;
	void *data;
};


enum parser_backend_type {
	PARSER_DTD,
	PARSER_CSS,
	PARSER_HTML,
};

struct parser_backend {
	void (*parse)(struct parser_state *, unsigned char **, int *);
};


/* The main parser entry point. If *state is NULL, it'll take str as the start
 * of the document and it will initialize the state, otherwise it will take str
 * as string immediatelly following the previous str supplied to
 * elusive_parser(). The str will usually end up pointing at the end of the
 * string, but it can point at few chars before that point, if it's needed for
 * an intermediate state (ie. while parsing tag content) (true, that isn't so
 * true lately, but ie. memory allocation failures handling rely on this
 * behaviour). */
void
elusive_parser(enum parser_backend_type parser, struct parser_state **state,
		unsigned char **str, int *len);

#endif
