/* $Id: document.h,v 1.1 2003/10/29 16:10:29 jonas Exp $ */

#ifndef EL__DOCUMENT_DOCUMENT_H
#define EL__DOCUMENT_DOCUMENT_H

#include "document/html/frames.h"
#include "document/options.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/lists.h"

struct line {
	struct screen_char *d;
	int l;
};

enum cp_status {
	CP_STATUS_NONE,
	CP_STATUS_SERVER,
	CP_STATUS_ASSUMED,
	CP_STATUS_IGNORED
};

struct document_refresh {
	int timer;
	unsigned long seconds;
	unsigned char url[1]; /* XXX: Keep last! */
};

struct link;
struct search;

struct document {
	LIST_HEAD(struct document);

	struct document_options opt;

	struct list_head forms;
	struct list_head tags;
	struct list_head nodes;

	unsigned char *url;
	unsigned char *title;

	struct frameset_desc *frame_desc;
	struct document_refresh *refresh;

	struct line *data;

	struct link *links;
	struct link **lines1;
	struct link **lines2;

	struct search *search;
	struct search **slines1;
	struct search **slines2;

	unsigned int id_tag; /* Used to check cache entries. */

	int refcount;
	int cp;
	int x, y; /* size of document */
	int nlinks;
	int nsearch;
	color_t bgcolor;

	enum cp_status cp_status;
};

#define document_has_frames(document_) ((document_) && (document_)->frame_desc)

#endif
