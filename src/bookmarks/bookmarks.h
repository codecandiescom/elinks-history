/* $Id: bookmarks.h,v 1.17 2003/04/24 08:23:38 zas Exp $ */

#ifndef EL__BOOKMARKS_BOOKMARKS_H
#define EL__BOOKMARKS_BOOKMARKS_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "util/lists.h"

/* Bookmark record structure */
struct bookmark {
	LIST_HEAD(struct bookmark);

	struct bookmark *root;
	struct list_head child;

	unsigned char *title;   /* title of bookmark */
	unsigned char *url;     /* Location of bookmarked item */
	int refcount;		/* Isn't anything else using this item now? */

	/* This is indeed maintained by bookmarks.c, not dialogs.c; much easier
	 * and simpler. */
	struct listbox_item *box_item;
};

extern struct list_head bookmarks;
extern struct list_head bookmark_box_items;
extern struct list_head bookmark_boxes;

extern int bookmarks_dirty;

/* Read/write bookmarks functions */
void read_bookmarks();
void write_bookmarks();

/* Cleanups and saves bookmarks */
void finalize_bookmarks();

int delete_bookmark(struct bookmark *);
struct bookmark *add_bookmark(struct bookmark *, int, const unsigned char *, const unsigned char *);
int update_bookmark(struct bookmark *, const unsigned char *, const unsigned char *);

#endif
