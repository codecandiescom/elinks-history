/* Layouter frontend */
/* $Id: layouter.c,v 1.3 2002/12/30 18:15:09 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/layouter/layouter.h"
#include "elusive/layouter/rectangle.h"
#include "elusive/parser/parser.h"
#include "util/memory.h"


struct layouter_backend *layouter_backends[] = {
	NULL,
	NULL,
	NULL,
};


struct layouter_state *
elusive_layouter_init(enum layouter_backend_type layouter,
			enum parser_backend_type parser)
{
	struct layouter_state *state;

	state = mem_calloc(1, sizeof(struct layouter_state));
	if (!state) return NULL;

	state->real_root = init_layout_rectangle();
	state->current = state->root = state->real_root;
	state->layouter = layouter;
	state->parser = parser;

	if (layouter_backends[state->layouter] &&
	    layouter_backends[state->layouter]->init)
		layouter_backends[state->layouter]->init(state);

	return state;
}

void
elusive_layouter_layout(struct layouter_state *state, unsigned char **str, int *len)
{
	if (layouter_backends[state->layouter] &&
	    layouter_backends[state->layouter]->layout)
		layouter_backends[state->layouter]->layout(state, str, len);
}

void
elusive_layouter_done(struct layouter_state *state)
{
	if (layouter_backends[state->layouter] &&
	    layouter_backends[state->layouter]->done)
		layouter_backends[state->layouter]->done(state);

	done_layout_rectangle(state->real_root);
	mem_free(state);
}
