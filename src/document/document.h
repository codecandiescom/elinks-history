/* $Id: document.h,v 1.39 2003/12/01 18:13:21 jonas Exp $ */

#ifndef EL__DOCUMENT_DOCUMENT_H
#define EL__DOCUMENT_DOCUMENT_H

#include "document/options.h"
#include "util/color.h"
#include "util/lists.h"

struct screen_char;
struct cache_entry;


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
	struct screen_char *chars;
	int length;
};

enum cp_status {
	CP_STATUS_NONE,
	CP_STATUS_SERVER,
	CP_STATUS_ASSUMED,
	CP_STATUS_IGNORED
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

struct form_control;

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

/* TODO: Move here? --jonas */
struct frameset_desc;

struct document_refresh;

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

struct document *get_cached_document(unsigned char *uristring, struct document_options *options, unsigned int id);

/* Release a reference to the document. */
void release_document(struct document *document);

long formatted_info(int);

void shrink_format_cache(int);
void count_format_cache(void);

void init_documents(void);
void done_documents(void);

#endif
