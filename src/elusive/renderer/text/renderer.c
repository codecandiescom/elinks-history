/* Text-only output renderer */
/* $Id: renderer.c,v 1.2 2003/01/01 14:28:01 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

/* TODO: Move that stuff to the viewer interface. We will first need to
 * sanitize the viewer, though. Or, better, move that stuff here, so that
 * multiple viewers (of other people using ELusive) could use this interface
 * as well. --pasky */
#include "document/html/renderer.h"

#include "elusive/layouter/syntree/layouter.h"
#include "elusive/parser/attrib.h"
#include "elusive/parser/syntree.h"
#include "elusive/renderer/renderer.h"
#include "util/memory.h"


/* TODO: Support for incremental rendering! --pasky */


static void
text_init(struct renderer_state *state)
{
	state->layouter_state = elusive_layouter_init(state->layouter,
							state->parser);
}

static void
text_render(struct renderer_state *state, unsigned char **str, int *len)
{
	elusive_layouter_layout(state->layouter_state, str, len);
	/* TODO: Ehm. */
}

static void
text_done(struct renderer_state *state)
{
	elusive_layouter_done(state->layouter_state);
}


struct renderer_backend text_renderer_backend = {
	text_init,
	text_render,
	text_done,
};
