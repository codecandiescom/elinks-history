/* $Id: view.h,v 1.17 2003/12/01 14:33:20 pasky Exp $ */

#ifndef EL__DOCUMENT_VIEW_H
#define EL__DOCUMENT_VIEW_H

#include "terminal/draw.h"
#include "util/lists.h"

struct document;
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

#endif
