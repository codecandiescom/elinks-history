/* $Id: bookmarks.h,v 1.12 2002/08/30 23:21:02 pasky Exp $ */

#ifndef EL__BOOKMARKS_BOOKMARKS_H
#define EL__BOOKMARKS_BOOKMARKS_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "util/lists.h"

/* Bookmark record structure */
struct bookmark {
	struct bookmark *next;
	struct bookmark *prev;

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

/* Search memorization */
extern unsigned char *bm_last_searched_name;
extern unsigned char *bm_last_searched_url;

/* Read/write bookmarks functions */
void read_bookmarks();
void write_bookmarks();

/* Cleanups and saves bookmarks */
void finalize_bookmarks();

int delete_bookmark(struct bookmark *);
struct bookmark *add_bookmark(const unsigned char *, const unsigned char *);
int update_bookmark(struct bookmark *, const unsigned char *, const unsigned char *);

int bookmark_simple_search(unsigned char *, unsigned char *);

#endif
