/* $Id: view.h,v 1.15 2003/10/31 22:37:17 pasky Exp $ */

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

#endif
