/* $Id: layouter.h,v 1.3 2002/12/31 00:48:54 pasky Exp $ */

#ifndef EL__USIVE_LAYOUTER_LAYOUTER_H
#define EL__USIVE_LAYOUTER_LAYOUTER_H

/* This is a universal layouter interface, as exposed to ELinks (in the ideal
 * case). */

#include "elusive/layouter/rectangle.h"
#include "elusive/parser/parser.h"


enum layouter_backend_type {
	LAYOUTER_HTML,
	LAYOUTER_SYNTREE,
};

/* This is the ELusive layouter state - it is created by init() and destroyed
 * by done(). Note that the ELusive layouter works perfectly with multiple
 * states - it will maintain the complete context per-sate, so you can layout
 * various sources independently at once. You then call init() and done() for
 * each of the states independently. */

struct layouter_state {
	enum layouter_backend_type layouter;
	void *data;

	enum parser_backend_type parser;
	/* This is managed per-backend, since some backends might not care
	 * about parsers at all. */
	struct parser_state *parser_state;

	struct layout_rectangle *real_root;
	struct layout_rectangle *root;
	struct layout_rectangle *current;
};

struct layouter_backend {
	void (*init)(struct layouter_state *);
	void (*layout)(struct layouter_state *, unsigned char **, int *);
	void (*done)(struct layouter_state *);
};


/* Initialize the ELusive layouting engine. Returns NULL if failed. */
struct layouter_state *
elusive_layouter_init(enum layouter_backend_type layouter,
			enum parser_backend_type parser);

/* Layout the supplied snippet of source, in the context of state (thus also
 * previously parser source snippets). The str will usually end up pointing at
 * the end of the string, but it can point at few chars before that point, if
 * it's needed for an intermediate state (ie. while parsing tag content). */
void
elusive_layouter_layout(struct layouter_state *state, unsigned char **str, int *len);

/* Shutdown the ELusive layouting engine. */
void
elusive_layouter_done(struct layouter_state *state);

#endif
