/* $Id: document.h,v 1.14 2003/10/30 18:30:31 jonas Exp $ */

#ifndef EL__DOCUMENT_DOCUMENT_H
#define EL__DOCUMENT_DOCUMENT_H

#include "document/html/frames.h"
#include "document/options.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/lists.h"

/* TODO: Move this notice to doc/hacking.txt */
/* XXX: Please try to keep order of fields from max. to min. of size
 * of each type of fields:
 *
 * Prefer:
 *	long a;
 *	int b;
 *	char c;
 * Instead of:
 *	char c;
 *	int b;
 *	long b;
 *
 * It will help to reduce memory padding on some architectures.
 * It's not a perfect solution, but better than worse.
 */

struct tag {
	LIST_HEAD(struct tag);

	int x, y;
	unsigned char name[1]; /* must be last of struct. --Zas */
};

struct node {
	LIST_HEAD(struct node);

	int x, y;
	int width, height;
};


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

enum link_type {
	LINK_HYPERTEXT,
	LINK_BUTTON,
	LINK_CHECKBOX,
	LINK_SELECT,
	LINK_FIELD,
	LINK_AREA,
};

#define link_is_textinput(link) \
	((link)->type == LINK_FIELD || (link)->type == LINK_AREA)

struct point {
	int x, y;
};

struct link {
	long accesskey;

	enum link_type type;

	unsigned char *where;
	unsigned char *target;
	unsigned char *where_img;
	unsigned char *title;
	unsigned char *name;

	struct form_control *form;
	struct point *pos;

	int n;
	int num;

	struct color_pair color;
};

struct search {
	int x, y;
	signed int n:24;	/* This structure is size-critical */
	unsigned char c;
};

struct document {
	LIST_HEAD(struct document);

	struct document_options options;

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
	int width, height; /* size of document */
	int nlinks;
	int nsearch;
	color_t bgcolor;

	enum cp_status cp_status;
};

#define document_has_frames(document_) ((document_) && (document_)->frame_desc)

/* Initializes a document and it's canvas. The @uristring should match a
 * cache_entry. */
/* Return NULL on allocation failure. */
struct document *
init_document(unsigned char *uristring, struct document_options *options);

/* Releases the document and all it's resources. */
void done_document(struct document *document);

struct document *get_cached_document(unsigned char *uristring, struct document_options *options, int id);

/* Release a reference to the document. */
void release_document(struct document *document);

long formatted_info(int);

void shrink_format_cache(int);
void count_format_cache(void);
void delete_unused_format_cache_entries(void);
void format_cache_reactivate(struct document *);

#endif
