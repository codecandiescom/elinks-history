/* $Id: view.h,v 1.23 2004/09/21 22:37:37 jonas Exp $ */

#ifndef EL__DOCUMENT_VIEW_H
#define EL__DOCUMENT_VIEW_H

#include "terminal/draw.h"
#include "util/lists.h"
#include "util/box.h"


struct document;
struct ecmascript_interpreter;
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
#ifdef CONFIG_ECMASCRIPT
	struct ecmascript_interpreter *ecmascript;
#endif

	int link_bg_n;
	struct box box;	/* pos and size of window */
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
