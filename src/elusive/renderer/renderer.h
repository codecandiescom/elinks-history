/* $Id: renderer.h,v 1.1 2002/12/31 01:37:00 pasky Exp $ */

#ifndef EL__USIVE_RENDERER_RENDERER_H
#define EL__USIVE_RENDERER_RENDERER_H

/* This is a universal renderer interface, as exposed to ELinks (in the ideal
 * case). */

#include "elusive/layouter/layouter.h"
#include "elusive/parser/parser.h"


enum renderer_backend_type {
	LAYOUTER_TEXT,
	LAYOUTER_GR,
};

/* This is the ELusive renderer state - it is created by init() and destroyed
 * by done(). Note that the ELusive renderer works perfectly with multiple
 * states - it will maintain the complete context per-state, so you can render
 * various layouts independently at once. You then call init() and done() for
 * each of the states independently. */

struct renderer_state {
	enum renderer_backend_type renderer;
	void *data;

	/* This pointer should contain data for the viewer (typically set of
	 * objects in form ready for direct displaying, array of forms and
	 * links, and so on). */
	void *output;

	enum parser_backend_type parser;
	enum layouter_backend_type layouter;
	/* This is managed per-backed, since some backends might not care
	 * about layouters at all (hrmpf.. show me any ;] --pasky). */
	struct layouter_state *layouter_state;
};

struct renderer_backend {
	void (*init)(struct renderer_state *);
	void (*render)(struct renderer_state *, unsigned char **, int *);
	void (*done)(struct renderer_state *);
};


/* Initialize the ELusive rendering engine. Returns NULL if failed. */
struct renderer_state *
elusive_renderer_init(enum renderer_backend_type renderer,
			enum layouter_backend_type layouter,
			enum parser_backend_type parser);

/* Render the supplied snippet of source, in the context of state (thus also
 * previously parser source snippets). The str will usually end up pointing at
 * the end of the string, but it can point at few chars before that point, if
 * it's needed for an intermediate state (ie. while parsing tag content). */
void
elusive_renderer_layout(struct renderer_state *state, unsigned char **str, int *len);

/* Shutdown the ELusive rendering engine. */
void
elusive_renderer_done(struct renderer_state *state);

#endif
