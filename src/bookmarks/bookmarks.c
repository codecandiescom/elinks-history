/* Internal bookmarks support */
/* $Id: bookmarks.c,v 1.86 2003/10/25 16:38:42 jonas Exp $ */

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
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"

#ifdef BOOKMARKS

/* The list of bookmarks */
INIT_LIST_HEAD(bookmarks);
INIT_LIST_HEAD(bookmark_box_items);
INIT_LIST_HEAD(bookmark_boxes);

/* Set to 1, if bookmarks have changed. */
int bookmarks_dirty = 0;




/* Life functions */

static struct option_info bookmark_options_info[] = {
	INIT_OPT_TREE("", N_("Bookmarks"),
		"bookmarks", 0,
		N_("Bookmark options.")),

#ifdef HAVE_LIBEXPAT
	INIT_OPT_INT("bookmarks", N_("File format"),
		"file_format", 0, 0, 1, 0,
		N_("File format for bookmarks (affects both reading and saving):\n"
		"0 is the default ELinks (Links 0.9x compatible) format\n"
		"1 is XBEL universal XML bookmarks format (NO NATIONAL CHARS SUPPORT!)")),
#else
	INIT_OPT_INT("bookmarks", N_("File format"),
		"file_format", 0, 0, 1, 0,
		N_("File format for bookmarks (affects both reading and saving):\n"
		"0 is the default ELinks (Links 0.9x compatible) format\n"
		"1 is XBEL universal XML bookmarks format (NO NATIONAL CHARS SUPPORT!)"
		"  (DISABLED)")),
#endif

	NULL_OPTION_INFO
};

static void
init_bookmarks(struct module *module)
{
	read_bookmarks();
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
static void
done_bookmarks(struct module *module)
{
	write_bookmarks();
	free_bookmarks(&bookmarks, &bookmark_box_items);
	if (bm_last_searched_name) mem_free(bm_last_searched_name);
	if (bm_last_searched_url) mem_free(bm_last_searched_url);
}

struct module bookmarks_module = INIT_MODULE(
	/* name: */		"bookmarks",
	/* options: */		bookmark_options_info,	
	/* submodules: */	NULL,
	/* init: */		init_bookmarks,	
	/* done: */		done_bookmarks
);



/* Read/write wrappers */

/* Loads the bookmarks from file */
void
read_bookmarks(void)
{
	bookmarks_read();
}

void
write_bookmarks(void)
{
	bookmarks_write(&bookmarks);
}




/* Bookmarks manipulation */

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

/* Replace invalid chars in @title with ' ' and trim all starting/ending
 * spaces. */
static inline void
sanitize_title(const unsigned char *title)
{
	register unsigned char *p = (unsigned char *)title;
	int len = strlen(p);

	if (!len) return;

	while (len--) {
		if (p[len] < ' ')
			p[len] = ' ';
	}
	trim_chars(p, ' ', NULL);
}

/* Returns 0 if @url contains invalid chars, 1 if ok.
 * It trims starting/ending spaces. */
static inline int
sanitize_url(const unsigned char *url)
{
	register unsigned char *p = (unsigned char *)url;
	int len = strlen(p);

	if (!len) return 1;

	while (len--) {
		if (p[len] < ' ')
			return 0;
	}
	trim_chars(p, ' ', NULL);
	return 1;
}

/* Adds a bookmark to the bookmark list. Place 0 means top, place 1 means
 * bottom. */
struct bookmark *
add_bookmark(struct bookmark *root, int place, const unsigned char *title,
	     const unsigned char *url)
{
	struct bookmark *bm;
	int title_size;

	if (!sanitize_url(url)) return NULL;

	bm = mem_alloc(sizeof(struct bookmark));
	if (!bm) return NULL;

	bm->title = stracpy((unsigned char *) title);
	if (!bm->title) {
		mem_free(bm);
		return NULL;
	}
	sanitize_title(bm->title);

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
	if (place) {
		if (root)
			add_to_list_end(root->child, bm);
		else
			add_to_list_end(bookmarks, bm);
	} else {
		if (root)
			add_to_list(root->child, bm);
		else
			add_to_list(bookmarks, bm);
	}
	bookmarks_dirty = 1;

	/* Setup box_item */
	/* Note that item_free is left at zero */
	title_size = strlen(bm->title) + 1;
	bm->box_item = mem_calloc(1, sizeof(struct listbox_item)
				     + title_size);
	if (!bm->box_item) return NULL;
	if (root) {
		bm->box_item->root = root->box_item;
		bm->box_item->depth = root->box_item->depth + 1;
	}
	init_list(bm->box_item->child);
	bm->box_item->visible = 1;

	bm->box_item->text = ((unsigned char *) bm->box_item
			      + sizeof(struct listbox_item));
	bm->box_item->box = &bookmark_boxes;
	bm->box_item->udata = (void *) bm;

	memcpy(bm->box_item->text, bm->title, title_size);

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

	if (url) {
		if (!sanitize_url(url)) return 0;

		url2 = stracpy((unsigned char *) url);
		if (!url2) return 0;
	}

	if (title) {
		title2 = stracpy((unsigned char *) title);
		if (!title2) {
			if (url2) mem_free(url2);
			return 0;
		}
		sanitize_title(title2);
	}

	if (title2) {
		struct listbox_item *b2;
		struct list_head *orig_child;
		int title_size = strlen(title2) + 1;

		orig_child = &bm->box_item->child;
		b2 = mem_realloc(bm->box_item,
				 sizeof(struct listbox_item)
				 + title_size);
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

		memcpy(bm->box_item->text, bm->title, title_size);
	}

	if (url2) {
		mem_free(bm->url);
		bm->url = url2;
	}

	bookmarks_dirty = 1;

	return 1;
}

#endif /* BOOKMARKS */
