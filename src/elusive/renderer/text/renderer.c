/* Text-only output renderer */
/* $Id: renderer.c,v 1.9 2003/01/17 23:16:15 pasky Exp $ */

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
#include "viewer/text/view.h"

#include "elusive/layouter/syntree/layouter.h"
#include "elusive/parser/property.h"
#include "elusive/parser/syntree.h"
#include "elusive/renderer/renderer.h"
#include "util/memory.h"


/* TODO: Support for incremental rendering! --pasky */


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


/* Now our strategy is really dumb and trivial, we allocate one line
 * per block box ;-). */
/* TODO: Reposition stuff, realign stuff, wrap stuff... ;-) --pasky */
static void
render_box(struct renderer_state *state, struct layout_box *box)
{
	struct f_data_c *console_frame_data = state->output;
	struct f_data *frame_data = console_frame_data->f_data;
	struct layout_box *leaf_box;

	switch (box->data_type) {
		case RECT_NONE:
			break;
		case RECT_TEXT:
			{
				struct layout_box_text *data = box->data;
				int y = frame_data->y, i;

				if (!strcmp(get_box_property(box, "display"),
					    "block")) {
					realloc_lines(frame_data, y + 1);
				}

				realloc_line(frame_data, y,
					     frame_data->data[y].l + data->len);

				for (i = frame_data->data[y].l - data->len;
				     i < data->len; i++) {
					frame_data->data[y].d[i] =
						data->str[i - frame_data->data[y].l];
				}
			}
			break;
	}

	foreach (leaf_box, box->leafs) {
		render_box(state, leaf_box);
	}
}


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
