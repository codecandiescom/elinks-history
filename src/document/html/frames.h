/* $Id: frames.h,v 1.34 2003/12/01 14:15:32 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_FRAMES_H
#define EL__DOCUMENT_HTML_FRAMES_H

#include "document/document.h"
#include "document/options.h"
#include "document/view.h"
#include "terminal/draw.h"
#include "util/ttime.h"

struct frameset_desc;

struct frame_desc {
	struct frameset_desc *subframe;

	unsigned char *name;
	unsigned char *url;

	int line;
	int width, height;
};

struct frameset_desc {
	int n;
	int x, y;
	int width, height;

	struct frame_desc frame_desc[1]; /* must be last of struct. --Zas */
};

struct frameset_param {
	struct frameset_desc *parent;
	int x, y;
	int *width, *height;
};

struct frameset_desc *create_frameset(struct frameset_param *fp);

/* Adds a frame to the @parent frameset. @subframe may be NULL. */
void
add_frameset_entry(struct frameset_desc *parent,
		   struct frameset_desc *subframe,
		   unsigned char *name, unsigned char *url);

void format_frames(struct session *ses, struct frameset_desc *fsd, struct document_options *op, int depth);

#endif
