/* $Id: frames.h,v 1.37 2004/04/04 04:44:48 jonas Exp $ */

#ifndef EL__DOCUMENT_HTML_FRAMES_H
#define EL__DOCUMENT_HTML_FRAMES_H

struct document_options;
struct session;
struct uri;

struct frameset_desc;

struct frame_desc {
	struct frameset_desc *subframe;

	unsigned char *name;
	struct uri *uri;

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

void parse_frame_widths(unsigned char *str, int max_value, int pixels_per_char,
		   int **new_values, int *new_values_count);

#endif
