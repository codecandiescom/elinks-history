/* $Id: document.h,v 1.26 2003/11/15 16:31:57 pasky Exp $ */

#ifndef EL__DOCUMENT_DOCUMENT_H
#define EL__DOCUMENT_DOCUMENT_H

#include "document/options.h"
#include "document/refresh.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/lists.h"


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

/* TODO: Move here? --jonas */
struct frameset_desc;

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

	int locks; /* No direct access, use provided macros for that. */
	int cp;
	int width, height; /* size of document */
	int nlinks;
	int nsearch;
	color_t bgcolor;

	enum cp_status cp_status;
};

#if 0
#define DEBUG_DOCUMENT_LOCKS
#endif

#ifdef DEBUG_DOCUMENT_LOCKS
#define doc_lock_debug(doc, info) debug("document %p lock %s now %d url= %s", doc, info, (doc)->locks, (doc)->url)
#else
#define doc_lock_debug(ce, info)
#endif

#ifdef DEBUG
#define doc_sanity_check(doc) do { assert(doc); assertm((doc)->locks >= 0, "Document lock underflow."); } while (0)
#else
#define doc_sanity_check(doc)
#endif

#define get_document_locks(doc) ((doc)->locks)
#define is_document_locked(doc) (!!(doc)->locks)
#define document_lock(doc) do { doc_sanity_check(doc); (doc)->locks++; doc_lock_debug(doc, "+1"); } while (0)
#define document_unlock(doc) do { (doc)->locks--; doc_lock_debug(doc, "-1"); doc_sanity_check(doc);} while (0)

/* Please keep this one. It serves for debugging. --Zas */
#define document_nolock(doc) do { doc_sanity_check(doc); doc_lock_debug(doc, "0"); } while (0)


#define document_has_frames(document_) ((document_) && (document_)->frame_desc)

/* Initializes a document and it's canvas. The @uristring should match a
 * cache_entry. */
/* Return NULL on allocation failure. */
struct document *
init_document(unsigned char *uristring, struct document_options *options);

/* Releases the document and all it's resources. */
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
