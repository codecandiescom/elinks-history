/* $Id: view.h,v 1.20 2004/05/10 17:15:22 zas Exp $ */

#ifndef EL__DOCUMENT_VIEW_H
#define EL__DOCUMENT_VIEW_H

#include "terminal/draw.h"
#include "util/lists.h"
#include "util/rect.h"


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
	struct rect dimensions;	/* pos and size of window */
	int last_x, last_y; /* last pos of window */
	int depth;
	int used;
};

#define get_current_link(doc_view) \
	(((doc_view) \
	  && (doc_view)->vs->current_link >= 0 \
	  && (doc_view)->vs->current_link < (doc_view)->document->nlinks) \
	? &(doc_view)->document->links[(doc_view)->vs->current_link] : NULL)

#endif
