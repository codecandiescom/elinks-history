/* $Id: document.h,v 1.50 2004/03/31 22:42:38 jonas Exp $ */

#ifndef EL__DOCUMENT_DOCUMENT_H
#define EL__DOCUMENT_DOCUMENT_H

#include "document/options.h"
#include "util/color.h"
#include "util/lists.h"

struct cache_entry;
struct document_refresh;
struct form_control;
struct frameset_desc;
struct module;
struct screen_char;
struct uri;


/* Tags are used for ``id''s or anchors in the document referenced by the
 * fragment part of the URI. */
struct tag {
	LIST_HEAD(struct tag);

	int x, y;
	unsigned char name[1]; /* must be last of struct. --Zas */
};

/* Nodes are used for marking areas of text on the document canvas as
 * searchable. */
struct node {
	LIST_HEAD(struct node);

	int x, y;
	int width, height;
};


/* The document line consisting of the chars ready to be copied to the terminal
 * screen. */
struct line {
	struct screen_char *chars;
	int length;
};

/* Codepage status */
enum cp_status {
	CP_STATUS_NONE,
	CP_STATUS_SERVER,
	CP_STATUS_ASSUMED,
	CP_STATUS_IGNORED
};


struct point {
	int x, y;
};


enum link_type {
	LINK_HYPERTEXT,
	LINK_BUTTON,
	LINK_CHECKBOX,
	LINK_SELECT,
	LINK_FIELD,
	LINK_AREA,
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

#define link_is_textinput(link) \
	((link)->type == LINK_FIELD || (link)->type == LINK_AREA)


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
	struct list_head css_imports;

	struct uri *uri;
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

	int refcount; /* No direct access, use provided macros for that. */
	int cp;
	int width, height; /* size of document */
	int nlinks;
	int nsearch;
	color_t bgcolor;

	enum cp_status cp_status;
};

#define document_has_frames(document_) ((document_) && (document_)->frame_desc)

/* Initializes a document and its canvas. */
/* Return NULL on allocation failure. */
struct document *
init_document(unsigned char *uri, struct cache_entry *cache_entry,
	      struct document_options *options);

/* Releases the document and all its resources. */
void done_document(struct document *document);

/* Free's the allocated members of the link. */
void done_link_members(struct link *link);

struct document *get_cached_document(unsigned char *uristring, struct document_options *options, unsigned int id);

/* Release a reference to the document. */
void release_document(struct document *document);

long formatted_info(int);

void shrink_format_cache(int);

extern struct module document_module;

#endif
