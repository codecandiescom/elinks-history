/* $Id: bookmarks.h,v 1.32 2004/05/30 02:46:38 jonas Exp $ */

#ifndef EL__BOOKMARKS_BOOKMARKS_H
#define EL__BOOKMARKS_BOOKMARKS_H

#ifdef CONFIG_BOOKMARKS

/* #include "bfu/listbox.h" */
struct listbox_item;
struct terminal;

#include "modules/module.h"
#include "util/lists.h"
#include "util/object.h"

/* Bookmark record structure */

struct bookmark {
	LIST_HEAD(struct bookmark);

	struct bookmark *root;

	/* This is indeed maintained by bookmarks.c, not dialogs.c; much easier
	 * and simpler. */
	struct listbox_item *box_item;

	unsigned char *title;   /* title of bookmark */
	unsigned char *url;     /* Location of bookmarked item */
	struct object object;	/* No direct access, use provided macros for that. */

	struct list_head child;
};

/* Bookmark lists */

extern struct list_head bookmarks; /* struct bookmark */

extern int bookmarks_dirty;

/* The bookmarks module */

extern struct module bookmarks_module;

/* Read/write bookmarks functions */

void read_bookmarks(void);
void write_bookmarks(void);

/* Bookmarks manipulation */

int delete_bookmark(struct bookmark *);
struct bookmark *add_bookmark(struct bookmark *, int, unsigned char *, unsigned char *);
struct bookmark *get_bookmark(unsigned char *url);
void bookmark_terminal_tabs(struct terminal *term, unsigned char *foldername);
int update_bookmark(struct bookmark *, unsigned char *, unsigned char *);
void open_bookmark_folder(struct session *ses, unsigned char *foldername);

#endif
#endif
