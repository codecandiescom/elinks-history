/* Text-only output renderer */
/* $Id: renderer.c,v 1.4 2003/01/01 17:18:02 pasky Exp $ */

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
#include "document/options.h"
#include "document/view.h"

#include "elusive/layouter/syntree/layouter.h"
#include "elusive/parser/attrib.h"
#include "elusive/parser/syntree.h"
#include "elusive/renderer/renderer.h"
#include "util/memory.h"


/* TODO: Support for incremental rendering! --pasky */


static void
text_init(struct renderer_state *state)
{
	struct document_options *document_options = state->input;
	struct f_data_c *console_frame_data;

	state->layouter_state = elusive_layouter_init(state->layouter,
							state->parser);

	state->output = mem_calloc(1, sizeof(struct f_data_c));
	if (!state->output) return;
	console_frame_data = state->output;

	console_frame_data->xw = document_options->xw;
	console_frame_data->yw = document_options->yw;
	console_frame_data->xp = document_options->xp;
	console_frame_data->yp = document_options->yp;

	console_frame_data->f_data = mem_calloc(1, sizeof(struct f_data));
	if (!console_frame_data->f_data) return;

	init_formatted(console_frame_data->f_data);
	console_frame_data->f_data->refcount = 1;
	copy_opt(&console_frame_data->f_data->opt, document_options);
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
