/* Text-only output renderer */
/* $Id: renderer.c,v 1.21 2003/07/15 20:18:09 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

/* TODO: Move that stuff to the viewer interface. We will first need to
 * sanitize the viewer, though. Or, better, move that stuff here, so that
 * multiple viewers (of other people using ELusive) could use this interface
 * as well. --pasky */
#include "document/html/renderer.h"
#include "document/options.h"
#include "terminal/draw.h"
#include "viewer/text/view.h"

#include "elusive/layouter/syntree/layouter.h"
#include "elusive/parser/property.h"
#include "elusive/parser/syntree.h"
#include "elusive/renderer/renderer.h"
#include "util/memory.h"


/* TODO: Support for incremental rendering! --pasky */


struct text_renderer_state {
	int last_display_type; /* 0 -> inline, 1 -> block */

	/* The reference box position and padding (absolute to the viewport).
	 * It's basically where we return with the virtual "cursor" when we
	 * move to a new line, where we even start with the drawing when we
	 * move to a new column, and what space we will leave at the
	 * bottom/right. */
	int leftpad;
	/* TODO: toppad,bottompad,rightpad */

	/* Position of the virtual "cursor" in the normal flow. */
	int x_pos;
	/* TODO: y_pos when we'll support non-static positioning! */
};


/* FIXME: Code duplication with Mikulas' renderer! */

#ifdef ALIGN
#undef ALIGN
#endif

#define ALIGN(x) (((x)+0x7f)&~0x7f)

static int
realloc_lines(struct document *document, int y)
{
	int i;
	int newsize = ALIGN(y + 1);

	if (newsize >= ALIGN(document->y)
	    && (!document->data || document->data->size < newsize)) {
		struct line *l;

		l = mem_realloc(document->data, newsize * sizeof(struct line));
		if (!l) return -1;

		document->data = l;
		document->data->size = newsize;
	}

	for (i = document->y; i <= y; i++) {
		document->data[i].l = 0;
		/* This should be sacrified on the altar of clarity. */
		document->data[i].c = 0;
		document->data[i].d = NULL;
	}

	document->y = i;

	return 0;
}

static int
realloc_line(struct document *document, int y, int x)
{
	int i;
	int newsize = ALIGN(x + 1);

	if (newsize >= ALIGN(document->data[y].l)
	    && (!document->data[y].d || document->data[y].dsize < newsize)) {
		chr *l;

		l = mem_realloc(document->data[y].d, newsize * sizeof(chr));
		if (!l) return -1;

		document->data[y].d = l;
		document->data[y].dsize = newsize;
	}

	document->data[y].c = 0;

	for (i = document->data[y].l; i <= x; i++) {
		document->data[y].d[i] = (document->data[y].c << 11) | ' ';
	}

	document->data[y].l = i;

	return 0;
}

#undef ALIGN

static void
put_text(struct document *frame_data, int x, int y, unsigned char *str, int len)
{
	int i;

	realloc_lines(frame_data, y);
	realloc_line(frame_data, y, x + len);

	for (i = 0; i < len; i++) {
		frame_data->data[y].d[x + i] = str[i];
	}
}


/* Now our strategy is really dumb and trivial, we allocate one line
 * per block box ;-). */
/* TODO: Reposition stuff, realign stuff, wrap stuff... ;-) --pasky */
static void
render_box(struct renderer_state *state, struct layout_box *box)
{
	struct text_renderer_state *rstate = state->data;
	struct document_view *console_frame_data = state->output;
	struct document *frame_data = console_frame_data->document;
	int y = frame_data->y;
	struct layout_box *leaf_box;
	int orig_leftpad = rstate->leftpad;

	{
		unsigned char *display = get_only_box_property(box, "display");

		if (display && !strcmp(display, "block")) {
			/* Don't make multiple newlines for subsequent block
			 * elements. */
			if (!rstate->last_display_type) {
				realloc_lines(frame_data, ++y);
				rstate->x_pos = rstate->leftpad;
			}
			rstate->last_display_type = 1;
		} else {
			rstate->last_display_type = 0;
		}
	}

	{
		unsigned char *leftpad;

		/* XXX: Assuming em unit. */
		leftpad = get_only_box_property(box, "padding-left");
		if (leftpad) {
			int lp = atoi(leftpad);

			rstate->leftpad += lp;
			rstate->x_pos += lp;
		}
	}

	y--;
	switch (box->data_type) {
		case RECT_NONE:
			break;
		case RECT_TEXT:
			{
				struct layout_box_text *data = box->data;

				put_text(frame_data, rstate->x_pos, y,
					data->str, data->len);

				rstate->x_pos += data->len;
			}
			break;
	}
	y++;

	foreach (leaf_box, box->leafs) {
		render_box(state, leaf_box);
	}

	rstate->leftpad = orig_leftpad;
}


static void
text_init(struct renderer_state *state)
{
	struct document_options *document_options = state->input;
	struct document_view *console_frame_data;

	state->data = mem_calloc(1, sizeof(struct text_renderer_state));
	if (!state->data) return;

	state->layouter_state = elusive_layouter_init(state->layouter,
							state->parser);

	state->output = mem_calloc(1, sizeof(struct document_view));
	if (!state->output) return;
	console_frame_data = state->output;

	console_frame_data->xw = document_options->xw;
	console_frame_data->yw = document_options->yw;
	console_frame_data->xp = document_options->xp;
	console_frame_data->yp = document_options->yp;

	console_frame_data->document = mem_calloc(1, sizeof(struct document));
	if (!console_frame_data->document) return;

	init_formatted(console_frame_data->document);
	console_frame_data->document->refcount = 1;
	copy_opt(&console_frame_data->document->opt, document_options);
}

static void
text_render(struct renderer_state *state, unsigned char **str, int *len)
{
	elusive_layouter_layout(state->layouter_state, str, len);

	render_box(state, state->layouter_state->real_root);
}

static void
text_done(struct renderer_state *state)
{
	struct document_view *console_frame_data = state->output;
	struct document *frame_data = console_frame_data->document;
	int i;

	elusive_layouter_done(state->layouter_state);

	for (i = 0; i < frame_data->y; i++)
		if (frame_data->data[i].l > frame_data->x)
			frame_data->x = frame_data->data[i].l;

}


struct renderer_backend text_renderer_backend = {
	text_init,
	text_render,
	text_done,
};
