/* Document view */
/* $Id: view.c,v 1.118 2003/10/31 01:59:39 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/document.h"
#include "document/html/frames.h"
#include "terminal/terminal.h"
#include "terminal/tab.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"
#include "viewer/text/link.h"
#include "viewer/text/form.h"
#include "viewer/text/search.h"
#include "viewer/text/vs.h"


void
done_document_view(struct document_view *doc_view)
{
	assert(doc_view);
	if_assert_failed return;

	if (doc_view->document) {
		release_document(doc_view->document);
		doc_view->document = NULL;
	}
	doc_view->vs = NULL;
	if (doc_view->link_bg) {
		mem_free(doc_view->link_bg), doc_view->link_bg = NULL;
		doc_view->link_bg_n = 0;
	}
	if (doc_view->name) {
		mem_free(doc_view->name), doc_view->name = NULL;
	}
}

static inline int
find_tag(struct document *document, unsigned char *name)
{
	struct tag *tag;

	foreach (tag, document->tags)
		if (!strcasecmp(tag->name, name))
			return tag->y;

	return -1;
}

static void
draw_frame_lines(struct terminal *t, struct frameset_desc *frameset_desc,
		 int xp, int yp)
{
	/* Optionalize? */
	struct color_pair colors = INIT_COLOR_PAIR(0x000000, 0xCCCCCC);
	register int y, j;

	assert(t && frameset_desc && frameset_desc->frame_desc);
	if_assert_failed return;

	y = yp - 1;
	for (j = 0; j < frameset_desc->height; j++) {
		register int x, i;
		int height = frameset_desc->frame_desc[j * frameset_desc->width].height;

		x = xp - 1;
		for (i = 0; i < frameset_desc->width; i++) {
			int width = frameset_desc->frame_desc[i].width;

			if (i) {
				draw_area(t, x, y + 1, 1, height, BORDER_SVLINE,
					  SCREEN_ATTR_FRAME, &colors);
				if (j == frameset_desc->height - 1)
					draw_border_cross(t, x, y + height + 1,
							  BORDER_X_UP, &colors);
			} else if (j) {
				if (x >= 0)
					draw_border_cross(t, x, y,
							  BORDER_X_RIGHT, &colors);
			}

			if (j) {
				draw_area(t, x + 1, y, width, 1, BORDER_SHLINE,
					  SCREEN_ATTR_FRAME, &colors);
				if (i == frameset_desc->width - 1
				    && x + width + 1 < t->width)
					draw_border_cross(t, x + width + 1, y,
							  BORDER_X_LEFT, &colors);
			} else if (i) {
				draw_border_cross(t, x, y, BORDER_X_DOWN, &colors);
			}

			if (i && j)
				draw_border_char(t, x, y, BORDER_SCROSS, &colors);

			x += width + 1;
		}
		y += height + 1;
	}

	y = yp - 1;
	for (j = 0; j < frameset_desc->height; j++) {
		register int x, i;
		int pj = j * frameset_desc->width;
		int height = frameset_desc->frame_desc[pj].height;

		x = xp - 1;
		for (i = 0; i < frameset_desc->width; i++) {
			int width = frameset_desc->frame_desc[i].width;
			int p = pj + i;

			if (frameset_desc->frame_desc[p].subframe) {
				draw_frame_lines(t, frameset_desc->frame_desc[p].subframe,
						 x + 1, y + 1);
			}
			x += width + 1;
		}
		y += height + 1;
	}
}

void
draw_document_view(struct document_view *doc_view, struct terminal *t, int active)
{
	struct color_pair color = INIT_COLOR_PAIR(0, 0);
	struct view_state *vs;
	int xp, yp;
	int width, height;
	int vx, vy;
	int y;

	assert(t && doc_view);
	if_assert_failed return;

	xp = doc_view->x;
	yp = doc_view->y;
	width = doc_view->width;
	height = doc_view->height;

	/* The code in this function assumes that both width and height are
	 * bigger than 1 so we have to bail out here. */
	if (width < 2 || height < 2) return;

	if (active) {
		set_cursor(t, xp + width - 1, yp + height - 1, 1);
		set_window_ptr(get_current_tab(t), xp, yp);
	}

	if (doc_view->document->height)
		color.background = doc_view->document->bgcolor;

	if (!doc_view->vs) {
		draw_area(t, xp, yp, width, height, ' ', 0, &color);
		return;
	}

	if (document_has_frames(doc_view->document)) {
	 	draw_area(t, xp, yp, width, height, ' ', 0, &color);
		draw_frame_lines(t, doc_view->document->frame_desc, xp, yp);
		if (doc_view->vs && doc_view->vs->current_link == -1)
			doc_view->vs->current_link = 0;
		return;
	}
	check_vs(doc_view);
	vs = doc_view->vs;
	if (vs->goto_position) {
		vy = find_tag(doc_view->document, vs->goto_position);
		if (vy != -1) {
			if (vy > doc_view->document->height)
				vy = doc_view->document->height - 1; /* XXX:zas: -1 ?? */
			if (vy < 0) vy = 0;
			vs->y = vy;
			set_link(doc_view);
			mem_free(vs->goto_position);
			vs->goto_position = NULL;
		}
	}
	vx = vs->x;
	vy = vs->y;
	if (doc_view->last_x != -1
	    && doc_view->last_x == vx
	    && doc_view->last_y == vy
	    && !has_search_word(doc_view)) {
		clear_link(t, doc_view);
		draw_forms(t, doc_view);
		if (active) draw_current_link(t, doc_view);
		return;
	}
	free_link(doc_view);
	doc_view->last_x = vx;
	doc_view->last_y = vy;
	draw_area(t, xp, yp, width, height, ' ', 0, &color);
	if (!doc_view->document->height) return;

	while (vs->y >= doc_view->document->height) vs->y -= height;
	if (vs->y < 0) vs->y = 0;
	if (vy != vs->y) vy = vs->y, check_vs(doc_view);
	for (y = int_max(vy, 0);
	     y < int_min(doc_view->document->height, height + vy);
	     y++) {
		int st = int_max(vx, 0);
		int en = int_min(doc_view->document->data[y].l, width + vx);

		if (en - st <= 0) continue;
		draw_line(t, xp + st - vx, yp + y - vy, en - st,
			  &doc_view->document->data[y].d[st]);
	}
	draw_forms(t, doc_view);
	if (active) draw_current_link(t, doc_view);
	if (has_search_word(doc_view))
		doc_view->last_x = doc_view->last_y = -1;
}
