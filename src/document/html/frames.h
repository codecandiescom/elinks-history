/* $Id: frames.h,v 1.28 2003/10/31 12:42:42 jonas Exp $ */

#ifndef EL__DOCUMENT_HTML_FRAMES_H
#define EL__DOCUMENT_HTML_FRAMES_H

#include "document/document.h"
#include "document/options.h"
#include "document/view.h"

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
