/* $Id: frames.h,v 1.18 2003/10/29 22:58:10 jonas Exp $ */

#ifndef EL__DOCUMENT_HTML_FRAMES_H
#define EL__DOCUMENT_HTML_FRAMES_H

#include "document/document.h"
#include "document/options.h"
#include "lowlevel/ttime.h"
#include "terminal/draw.h"

struct frameset_desc;

struct frame_desc {
	struct frameset_desc *subframe;

	unsigned char *name;
	unsigned char *url;

	int line;
	int xw, yw;
};

struct frameset_desc {
	int n;
	int x, y;
	int xp, yp;

	struct frame_desc f[1]; /* must be last of struct. --Zas */
};

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
	int xw, yw; /* size of window */
	int xp, yp; /* pos of window */
	int xl, yl; /* last pos of window */
	int depth;
	int used;
};

struct frameset_param {
	struct frameset_desc *parent;
	int x, y;
	int *xw, *yw;
};

struct frame_param {
	struct frameset_desc *parent;
	unsigned char *name;
	unsigned char *url;
};

struct frameset_desc *create_frameset(struct document *doc, struct frameset_param *fp);
void create_frame(struct frame_param *fp);
struct document_view *format_frame(struct session *ses, unsigned char *name, struct document_options *o, int depth);
void format_frames(struct session *ses, struct frameset_desc *fsd, struct document_options *op, int depth);
#define document_has_frames(document_) ((document_) && (document_)->frame_desc)

#endif
