/* Renderer frontend */
/* $Id: renderer.c,v 1.2 2002/12/31 01:44:04 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/layouter/layouter.h"
#include "elusive/parser/parser.h"
#include "elusive/renderer/renderer.h"
#include "util/memory.h"


#include "elusive/renderer/text/renderer.h"

struct renderer_backend *renderer_backends[] = {
	text_renderer_backend,
	NULL,
};


struct renderer_state *
elusive_renderer_init(enum renderer_backend_type renderer,
			enum layouter_backend_type layouter,
			enum parser_backend_type parser)
{
	struct renderer_state *state;

	state = mem_calloc(1, sizeof(struct renderer_state));
	if (!state) return NULL;

	state->renderer = renderer;
	state->layouter = layouter;
	state->parser = parser;

	if (renderer_backends[state->renderer] &&
	    renderer_backends[state->renderer]->init)
		renderer_backends[state->renderer]->init(state);

	return state;
}

void
elusive_renderer_layout(struct renderer_state *state, unsigned char **str, int *len)
{
	if (renderer_backends[state->renderer] &&
	    renderer_backends[state->renderer]->render)
		renderer_backends[state->renderer]->render(state, str, len);
}

void
elusive_renderer_done(struct renderer_state *state)
{
	if (renderer_backends[state->renderer] &&
	    renderer_backends[state->renderer]->done)
		renderer_backends[state->renderer]->done(state);

	mem_free(state);
}
