/* Text-only output renderer */
/* $Id: renderer.c,v 1.1 2002/12/31 01:44:04 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

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
text_layout(struct renderer_state *state, unsigned char **str, int *len)
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
	text_layout,
	text_done,
};
