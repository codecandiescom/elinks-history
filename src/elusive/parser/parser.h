/* $Id: parser.h,v 1.5 2002/12/30 02:04:45 pasky Exp $ */

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

/* This is the ELusive parser state - it is created by elusive_init() and
 * destroyed by elusive_done(). Note that the ELusive parser works perfectly
 * with multiple states - it will maintain the complete context per-sate, so
 * you can parse various sources independently at once. You then call init()
 * and done() for each of the states independently. */

struct parser_state {
	enum parser_backend_type parser;
	void *data;

	struct syntree_node *real_root;
	struct syntree_node *root;
	struct syntree_node *current;
};

struct parser_backend {
	void (*init)(struct parser_state *);
	void (*parse)(struct parser_state *, unsigned char **, int *);
	void (*done)(struct parser_state *);
};


/* Initialize the ELusive parsing engine. Returns NULL if failed. */
struct parser_state *
elusive_init(enum parser_backend_type parser);

/* Parse the supplied snippet of source, in the context of state (thus also
 * previously parser source snippets). The str will usually end up pointing at
 * the end of the string, but it can point at few chars before that point, if
 * it's needed for an intermediate state (ie. while parsing tag content). */
void
elusive_parse(struct parser_state *state, unsigned char **str, int *len);

/* Shutdown the ELusive parsing engine. */
void
elusive_done(struct parser_state *state);

#endif
