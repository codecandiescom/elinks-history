/* Internal bookmarks support */
/* $Id: bookmarks.c,v 1.60 2002/12/05 21:30:05 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "bfu/listbox.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "bookmarks/backend/common.h"
#include "lowlevel/home.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"

/* The list of bookmarks */
struct list_head bookmarks = { &bookmarks, &bookmarks };
struct list_head bookmark_box_items = { &bookmark_box_items,
					&bookmark_box_items };
struct list_head bookmark_boxes = { &bookmark_boxes, &bookmark_boxes };


#ifdef BOOKMARKS

/* Set to 1, if bookmarks have changed. */
int bookmarks_dirty = 0;


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
	mem_free(bm->box_item);
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

	bm->root = root;
	init_list(bm->child);

	bm->refcount = 0;

	/* Actually add it */
	/* add_at_pos() is here to add it at the _end_ of the list,
	 * not vice versa. */
	if (place)
	add_at_pos((struct bookmark *) (root ? root->child.prev
					      : bookmarks.prev),
		   bm);
	else
		add_to_list((root ? root->child : bookmarks), bm);
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

	if (place)
	add_at_pos((struct listbox_item *) (root ? root->box_item->child.prev
						 : bookmark_box_items.prev),
		   bm->box_item);
	else
		add_to_list((root ? root->box_item->child : bookmark_box_items),
			    bm->box_item);

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
	if (title) {
		mem_free(bm->title);
		bm->title = stracpy((unsigned char *) title);
		if (!bm->title) return 0;

		bm->box_item = mem_realloc(bm->box_item,
					   sizeof(struct listbox_item)
					   + strlen(bm->title) + 1);
		if (!bm->box_item) {
			mem_free(bm->title);
			return 0;
		}
		strcpy(bm->box_item->text, bm->title);
	}

	if (url) {
		mem_free(bm->url);
		bm->url = stracpy((unsigned char *) url);
		if (!bm->url) {
			if (title) {
				mem_free(bm->title);
				mem_free(bm->box_item);
			}
			return 0;
		}
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
void
free_bookmarks(struct list_head *bookmarks, struct list_head *box_items)
{
	struct bookmark *bm;

	foreach (bm, *bookmarks) {
		if (!list_empty(bm->child))
			free_bookmarks(&bm->child, &bm->box_item->child);
		mem_free(bm->title);
		mem_free(bm->url);
	}

	free_list(*box_items);
	free_list(*bookmarks);
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
