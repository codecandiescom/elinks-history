/* $Id: bookmarks.h,v 1.4 2002/04/01 22:20:00 pasky Exp $ */

#ifndef EL__BOOKMARKS_H
#define EL__BOOKMARKS_H

#include <links.h> /* list_head */

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
	int selected;           /* Whether to display this bookmark or not */
};

extern struct list_head bookmarks;

/* Search memorization */
extern unsigned char *bm_last_searched_name;
extern unsigned char *bm_last_searched_url;

/* Read/write bookmarks functions */
void read_bookmarks();
/* void write_bookmarks(); */

/* Cleanups and saves bookmarks */
void finalize_bookmarks();

struct bookmark *get_bookmark_by_id(bookmark_id);
int delete_bookmark_by_id(bookmark_id);
void add_bookmark(const unsigned char *, const unsigned char *);
int update_bookmark(bookmark_id, const unsigned char *, const unsigned char *);

int bookmark_simple_search(unsigned char *, unsigned char *);

#endif
