/* $Id: bookmarks.h,v 1.22 2003/10/25 10:31:26 pasky Exp $ */

#ifndef EL__BOOKMARKS_BOOKMARKS_H
#define EL__BOOKMARKS_BOOKMARKS_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "util/lists.h"

/* Bookmark record structure */

struct bookmark {
	LIST_HEAD(struct bookmark);

	struct bookmark *root;

	/* This is indeed maintained by bookmarks.c, not dialogs.c; much easier
	 * and simpler. */
	struct listbox_item *box_item;

	unsigned char *title;   /* title of bookmark */
	unsigned char *url;     /* Location of bookmarked item */
	int refcount;		/* Isn't anything else using this item now? */

	struct list_head child;
};

/* Bookmark lists */

extern struct list_head bookmarks; /* struct bookmark */
extern struct list_head bookmark_box_items; /* struct listbox_item */
extern struct list_head bookmark_boxes; /* struct listbox_data */

extern int bookmarks_dirty;

/* Life functions */

void init_bookmarks(void);
void done_bookmarks(void);

/* Read/write bookmarks functions */

void read_bookmarks(void);
void write_bookmarks(void);

/* Bookmarks manipulation */

int delete_bookmark(struct bookmark *);
struct bookmark *add_bookmark(struct bookmark *, int, const unsigned char *, const unsigned char *);
int update_bookmark(struct bookmark *, const unsigned char *, const unsigned char *);

#endif
