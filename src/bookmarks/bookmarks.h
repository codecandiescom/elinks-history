/* $Id: bookmarks.h,v 1.9 2002/08/29 23:26:01 pasky Exp $ */

#ifndef EL__BOOKMARKS_BOOKMARKS_H
#define EL__BOOKMARKS_BOOKMARKS_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "util/lists.h"

/* A pointer independent id that bookmarks can be identified by. Guarenteed to
 * be unique between all bookmarks. */
typedef int bookmark_id;
#define BAD_BOOKMARK_ID ((bookmark_id) -1)

/* Bookmark record structure */
struct bookmark {
	struct bookmark *next;
	struct bookmark *prev;

	bookmark_id id;         /* Bookmark id */
	unsigned char *title;   /* title of bookmark */
	unsigned char *url;     /* Location of bookmarked item */
	
	/* This is indeed maintained by bookmarks.c, not dialogs.c; much easier
	 * and simpler. */
	struct listbox_item *box_item;
};

extern struct list_head bookmarks;
extern struct list_head bookmark_box_items;

/* Search memorization */
extern unsigned char *bm_last_searched_name;
extern unsigned char *bm_last_searched_url;

/* Read/write bookmarks functions */
void read_bookmarks();
void write_bookmarks();

/* Cleanups and saves bookmarks */
void finalize_bookmarks();

struct bookmark *get_bookmark_by_id(bookmark_id);
int delete_bookmark_by_id(bookmark_id);
struct bookmark *add_bookmark(const unsigned char *, const unsigned char *);
int update_bookmark(bookmark_id, const unsigned char *, const unsigned char *);

int bookmark_simple_search(unsigned char *, unsigned char *);

#endif
