/* Internal bookmarks support */
/* $Id: bookmarks.c,v 1.69 2003/04/28 15:37:31 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listbox.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "bookmarks/backend/common.h"
#include "lowlevel/home.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"

/* The list of bookmarks */
INIT_LIST_HEAD(bookmarks);
INIT_LIST_HEAD(bookmark_box_items);
INIT_LIST_HEAD(bookmark_boxes);

#ifdef BOOKMARKS

/* Set to 1, if bookmarks have changed. */
int bookmarks_dirty = 0;


/* FIXME: stupid workaround for bookmarks/globhist segfault */
/* I don't like this way, this workaround should be replaced by
 * an in-depth fix. */
static void
box_item_mem_free(void *p)
{
	static void *tmp = NULL;

	if (tmp) mem_free(tmp);
	tmp = p;
}

/* Deletes a bookmark. Returns 0 on failure (no such bm), 1 on success. */
int
delete_bookmark(struct bookmark *bm)
{
	if (!list_empty(bm->child)) {
		struct bookmark *bm2 = bm->child.next;

		while ((struct list_head *) bm2 != &bm->child) {
			struct bookmark *nbm = bm2->next;

			delete_bookmark(bm2);
			bm2 = nbm;
		}
	}

	del_from_list(bm);
	bookmarks_dirty = 1;

	/* Now wipe the bookmark */
	del_from_list(bm->box_item);
	/* FIXME: Segfault was caused by mem_free(bm->box_item).
	 * This pointer was needed in traverse_listbox_item_list.
	 * We suspend mem_free(), box_item is freed next time
	 * when it is no longer needed. */
	box_item_mem_free(bm->box_item);

	mem_free(bm->title);
	mem_free(bm->url);
	mem_free(bm);

	return 1;
}

/* Adds a bookmark to the bookmark list. Place 0 means top, place 1 means
 * bottom. */
struct bookmark *
add_bookmark(struct bookmark *root, int place, const unsigned char *title,
	     const unsigned char *url)
{
	unsigned char *p;
	int i;
	struct bookmark *bm = mem_alloc(sizeof(struct bookmark));

	if (!bm) return NULL;

	bm->title = stracpy((unsigned char *) title);
	if (!bm->title) {
		mem_free(bm);
		return NULL;
	}

	bm->url = stracpy((unsigned char *) url);
	if (!bm->url) {
		mem_free(bm->title);
		mem_free(bm);
		return NULL;
	}

	p = bm->title;
	for (i = strlen(p) - 1; i >= 0; i--)
		if (p[i] < ' ')
			p[i] = ' ';

	p = bm->url;
	for (i = strlen(p) - 1; i >= 0; i--)
		if (p[i] < ' ') {
			mem_free(bm->url);
			mem_free(bm->title);
			mem_free(bm);
			return NULL;
		}

	bm->root = root;
	init_list(bm->child);

	bm->refcount = 0;

	/* Actually add it */
	/* add_at_pos() is here to add it at the _end_ of the list,
	 * not vice versa. */
	if (place) {
		if (root)
			add_at_pos((struct bookmark *) root->child.prev, bm);
		else
			add_at_pos((struct bookmark *) bookmarks.prev, bm);
	} else {
		if (root)
			add_to_list(root->child, bm);
		else
			add_to_list(bookmarks, bm);
	}
	bookmarks_dirty = 1;

	/* Setup box_item */
	/* Note that item_free is left at zero */

	bm->box_item = mem_calloc(1, sizeof(struct listbox_item)
				     + strlen(bm->title) + 1);
	if (!bm->box_item) return NULL;
	bm->box_item->root = root ? root->box_item : NULL;
	bm->box_item->depth = root ? root->box_item->depth + 1 : 0;
	init_list(bm->box_item->child);
	bm->box_item->visible = 1;

	bm->box_item->text = ((unsigned char *) bm->box_item
			      + sizeof(struct listbox_item));
	bm->box_item->box = &bookmark_boxes;
	bm->box_item->udata = (void *) bm;

	strcpy(bm->box_item->text, bm->title);

	if (place) {
		if (root)
			add_at_pos((struct listbox_item *)
					root->box_item->child.prev,
					bm->box_item);
		else
			add_at_pos((struct listbox_item *)
					bookmark_box_items.prev,
					bm->box_item);
	} else {
		if (root)
			add_to_list(root->box_item->child, bm->box_item);
		else
			add_to_list(bookmark_box_items, bm->box_item);
	}

	return bm;
}

/* Updates an existing bookmark.
 *
 * If there's any problem, return 0. Otherwise, return 1.
 *
 * If any of the fields are NULL, the value is left unchanged. */
int
update_bookmark(struct bookmark *bm, const unsigned char *title,
		const unsigned char *url)
{
	unsigned char *title2 = NULL;
	unsigned char *url2 = NULL;

	if (title) {
		title2 = stracpy((unsigned char *) title);
		if (!title2) return 0;
	}

	if (url) {
		url2 = stracpy((unsigned char *) url);
		if (!url2) {
			if (title2) mem_free(title2);
			return 0;
		}
	}

	if (title2) {
		struct listbox_item *b2;
		struct list_head *orig_child;

		orig_child = &bm->box_item->child;
		b2 = mem_realloc(bm->box_item,
				 sizeof(struct listbox_item)
				 + strlen(title2) + 1);
		if (!b2) {
			mem_free(title2);
			if (url2) mem_free(url2);
			return 0;
		}

		mem_free(bm->title);
		bm->title = title2;

		if (b2 != bm->box_item) {
			struct listbox_item *item;
			struct listbox_data *box;

			/* We are being relocated, so update everything. */
			b2->next->prev = b2;
			b2->prev->next = b2;
			foreach (box, *b2->box) {
				if (box->sel == bm->box_item) box->sel = b2;
				if (box->top == bm->box_item) box->top = b2;
			}

			if (b2->child.next == orig_child) {
				b2->child.next = &b2->child;
				b2->child.prev = &b2->child;
			} else {
				((struct list_head *) b2->child.next)->prev = &b2->child;
				((struct list_head *) b2->child.prev)->next = &b2->child;
			}

			foreach (item, b2->child) {
				item->root = b2;
			}

			bm->box_item = b2;
			bm->box_item->text =
				((unsigned char *) bm->box_item
				 + sizeof(struct listbox_item));
		}

		strcpy(bm->box_item->text, bm->title);
	}

	if (url2) {
		mem_free(bm->url);
		bm->url = url2;
	}

	bookmarks_dirty = 1;

	return 1;
}


/* Loads the bookmarks from file */
void
read_bookmarks()
{
	bookmarks_read();
}

void
write_bookmarks()
{
	bookmarks_write(&bookmarks);
}

/* Clears the bookmark list */
static void
free_bookmarks(struct list_head *bookmarks_list,
	       struct list_head *box_items)
{
	struct bookmark *bm;

	foreach (bm, *bookmarks_list) {
		if (!list_empty(bm->child))
			free_bookmarks(&bm->child, &bm->box_item->child);
		mem_free(bm->title);
		mem_free(bm->url);
	}

	free_list(*box_items);
	free_list(*bookmarks_list);
}

/* Does final cleanup and saving of bookmarks */
void
finalize_bookmarks()
{
	write_bookmarks();
	free_bookmarks(&bookmarks, &bookmark_box_items);
	if (bm_last_searched_name) mem_free(bm_last_searched_name);
	if (bm_last_searched_url) mem_free(bm_last_searched_url);
}

#endif /* BOOKMARKS */
