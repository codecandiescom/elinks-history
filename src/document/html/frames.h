/* $Id: frames.h,v 1.1 2003/07/04 09:40:00 zas Exp $ */

#ifndef EL__DOCUMENT_HTML_FRAMES_H
#define EL__DOCUMENT_HTML_FRAMES_H

#include "document/options.h"
#include "lowlevel/ttime.h"
#include "terminal/draw.h" /* chr type */

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

/* For struct f_data */
struct line {
	chr *d;

	int l;
	int size;
	int dsize;

	chr c;
};
enum cp_status {
	CP_STATUS_NONE,
	CP_STATUS_SERVER,
	CP_STATUS_ASSUMED,
	CP_STATUS_IGNORED
};

struct f_data {
	LIST_HEAD(struct f_data);

	struct document_options opt;

	struct list_head forms;
	struct list_head tags;
	struct list_head nodes;

	unsigned char *url;
	unsigned char *title;

	struct frameset_desc *frame_desc;

	struct line *data;

	struct link *links;
	struct link **lines1;
	struct link **lines2;

	struct search *search;
	struct search **slines1;
	struct search **slines2;

	ttime time_to_get;
	unsigned int use_tag;

	int refcount;
	int cp;
	int x, y; /* size of document */
	int frame;
	int bg;
	int nlinks;
	int nsearch;

	enum cp_status cp_status;
};

#include "viewer/text/vs.h"

struct f_data_c {
	LIST_HEAD(struct f_data_c);

	unsigned char *name;
	unsigned char **search_word;

	struct f_data *f_data;
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

struct frameset_desc *create_frameset(struct f_data *fda, struct frameset_param *fp);
void create_frame(struct frame_param *fp);
struct f_data_c *format_frame(struct session *ses, unsigned char *name, struct document_options *o, int depth);
void format_frames(struct session *ses, struct frameset_desc *fsd, struct document_options *op, int depth);

#endif
