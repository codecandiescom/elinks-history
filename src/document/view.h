/* $Id: view.h,v 1.14 2003/10/31 22:33:50 pasky Exp $ */

#ifndef EL__DOCUMENT_VIEW_H
#define EL__DOCUMENT_VIEW_H

#include "document/document.h"
#include "document/options.h"
#include "terminal/draw.h"

struct view_state;

struct link_bg {
	int x, y;
	struct screen_char c;
};

struct document_view {
	LIST_HEAD(struct document_view);

	unsigned char *name;
	unsigned char **search_word;

	struct document *document;
	struct view_state *vs;
	struct link_bg *link_bg;

	int link_bg_n;
	int x, y; /* pos of window */
	int width, height; /* size of window */
	int last_x, last_y; /* last pos of window */
	int depth;
	int used;
};

/* Releases the document view's resources. But doesn't free() the @view. */
void done_document_view(struct document_view *doc_view);

/* Puts the formatted document on the given terminal's screen. */
void draw_document_view(struct document_view *doc_view, struct terminal *term, int active);

#endif
