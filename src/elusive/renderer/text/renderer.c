/* Text-only output renderer */
/* $Id: renderer.c,v 1.15 2003/01/19 14:36:10 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

/* TODO: Move that stuff to the viewer interface. We will first need to
 * sanitize the viewer, though. Or, better, move that stuff here, so that
 * multiple viewers (of other people using ELusive) could use this interface
 * as well. --pasky */
#include "document/html/renderer.h"
#include "document/options.h"
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
realloc_lines(struct f_data *f_data, int y)
{
	int i;
	int newsize = ALIGN(y + 1);

	if (newsize >= ALIGN(f_data->y)
	    && (!f_data->data || f_data->data->size < newsize)) {
		struct line *l;

		l = mem_realloc(f_data->data, newsize * sizeof(struct line));
		if (!l) return -1;

		f_data->data = l;
		f_data->data->size = newsize;
	}

	for (i = f_data->y; i <= y; i++) {
		f_data->data[i].l = 0;
		/* This should be sacrified on the altar of clarity. */
		f_data->data[i].c = 0;
		f_data->data[i].d = NULL;
	}

	f_data->y = i;

	return 0;
}

static int
realloc_line(struct f_data *f_data, int y, int x)
{
	int i;
	int newsize = ALIGN(x + 1);

	if (newsize >= ALIGN(f_data->data[y].l)
	    && (!f_data->data[y].d || f_data->data[y].dsize < newsize)) {
		chr *l;

		l = mem_realloc(f_data->data[y].d, newsize * sizeof(chr));
		if (!l) return -1;

		f_data->data[y].d = l;
		f_data->data[y].dsize = newsize;
	}

	f_data->data[y].c = 0;

	for (i = f_data->data[y].l; i <= x; i++) {
		f_data->data[y].d[i] = (f_data->data[y].c << 11) | ' ';
	}

	f_data->data[y].l = i;

	return 0;
}

#undef ALIGN

static void
put_text(struct f_data *frame_data, int x, int y, unsigned char *str, int len)
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
	struct f_data_c *console_frame_data = state->output;
	struct f_data *frame_data = console_frame_data->f_data;
	int y = frame_data->y;
	struct layout_box *leaf_box;
	int leftpad = rstate->leftpad;

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

			rstate->leftpad += lp; rstate->x_pos += lp;
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

	rstate->leftpad = leftpad;
}


static void
text_init(struct renderer_state *state)
{
	struct document_options *document_options = state->input;
	struct f_data_c *console_frame_data;

	state->data = mem_calloc(1, sizeof(struct text_renderer_state));
	if (!state->data) return;

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

	render_box(state, state->layouter_state->real_root);
}

static void
text_done(struct renderer_state *state)
{
	struct f_data_c *console_frame_data = state->output;
	struct f_data *frame_data = console_frame_data->f_data;
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
