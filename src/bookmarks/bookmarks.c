/* Internal bookmarks support */
/* $Id: bookmarks.c,v 1.118 2004/05/25 04:13:40 jonas Exp $ */

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
#include "sched/task.h"
#include "protocol/uri.h"
#include "terminal/tab.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"
#include "util/object.h"

/* The list of bookmarks */
INIT_LIST_HEAD(bookmarks);

/* Set to 1, if bookmarks have changed. */
int bookmarks_dirty = 0;




/* Life functions */

static struct option_info bookmark_options_info[] = {
	INIT_OPT_TREE("", N_("Bookmarks"),
		"bookmarks", 0,
		N_("Bookmark options.")),

#ifdef CONFIG_XBEL_BOOKMARKS
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
	free_bookmarks(&bookmarks, &bookmark_browser.root.child);
	mem_free_if(bm_last_searched_name);
	mem_free_if(bm_last_searched_url);
}

struct module bookmarks_module = struct_module(
	/* name: */		N_("Bookmarks"),
	/* options: */		bookmark_options_info,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
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
	if (get_opt_int_tree(cmdline_options, "anonymous")) {
		bookmarks_dirty = 0;
		return;
	}
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
	done_listbox_item(&bookmark_browser, bm->box_item);

	mem_free(bm->title);
	mem_free(bm->url);
	mem_free(bm);

	return 1;
}

/* Replace invalid chars in @title with ' ' and trim all starting/ending
 * spaces. */
static inline void
sanitize_title(unsigned char *title)
{
	int len = strlen(title);

	if (!len) return;

	while (len--) {
		if (title[len] < ' ')
			title[len] = ' ';
	}
	trim_chars(title, ' ', NULL);
}

/* Returns 0 if @url contains invalid chars, 1 if ok.
 * It trims starting/ending spaces. */
static inline int
sanitize_url(unsigned char *url)
{
	int len = strlen(url);

	if (!len) return 1;

	while (len--) {
		if (url[len] < ' ')
			return 0;
	}
	trim_chars(url, ' ', NULL);
	return 1;
}

/* Adds a bookmark to the bookmark list. Place 0 means top, place 1 means
 * bottom. NULL or "" @url means it is a bookmark folder. */
struct bookmark *
add_bookmark(struct bookmark *root, int place, unsigned char *title,
	     unsigned char *url)
{
	struct bookmark *bm;

	if (url && !sanitize_url(url)) return NULL;

	bm = mem_calloc(1, sizeof(struct bookmark));
	if (!bm) return NULL;

	bm->title = stracpy(title);
	if (!bm->title) {
		mem_free(bm);
		return NULL;
	}
	sanitize_title(bm->title);

	bm->url = stracpy(empty_string_or_(url));
	if (!bm->url) {
		mem_free(bm->title);
		mem_free(bm);
		return NULL;
	}

	bm->root = root;
	init_list(bm->child);

	object_nolock(bm, "bookmark");

	/* Actually add it */
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
	bm->box_item = mem_calloc(1, sizeof(struct listbox_item));
	if (!bm->box_item) return NULL;
	if (root) {
		bm->box_item->root = root->box_item;
		bm->box_item->depth = root->box_item->depth + 1;
	}
	init_list(bm->box_item->child);
	bm->box_item->visible = 1;

	bm->box_item->text = bm->title;
	bm->box_item->udata = (void *) bm;
	bm->box_item->type = (url && *url ? BI_LEAF : BI_FOLDER);

	if (place) {
		if (root)
			add_to_list_end(root->box_item->child,
					bm->box_item);
		else
			add_to_list_end(bookmark_browser.root.child,
					bm->box_item);
	} else {
		if (root)
			add_to_list(root->box_item->child, bm->box_item);
		else
			add_to_list(bookmark_browser.root.child, bm->box_item);
	}

	return bm;
}

/* Updates an existing bookmark.
 *
 * If there's any problem, return 0. Otherwise, return 1.
 *
 * If any of the fields are NULL, the value is left unchanged. */
int
update_bookmark(struct bookmark *bm, unsigned char *title,
		unsigned char *url)
{
	unsigned char *title2 = NULL;
	unsigned char *url2 = NULL;

	if (url) {
		if (!sanitize_url(url)) return 0;

		url2 = stracpy(url);
		if (!url2) return 0;
	}

	if (title) {
		title2 = stracpy(title);
		if (!title2) {
			mem_free_if(url2);
			return 0;
		}
		sanitize_title(title2);
	}

	if (title2) {
		mem_free(bm->title);
		bm->title = title2;
		bm->box_item->text = bm->title;
	}

	if (url2) {
		mem_free(bm->url);
		bm->url = url2;
	}

	bookmarks_dirty = 1;

	return 1;
}

void
bookmark_terminal_tabs(struct terminal *term, unsigned char *foldername)
{
	unsigned char title[MAX_STR_LEN], url[MAX_STR_LEN];
	struct bookmark *folder = NULL;
	struct bookmark *bookmark;
	struct window *tab;

	foreach (bookmark, bookmarks) {
		if (strcmp(bookmark->title, foldername))
			continue;
		folder = bookmark;
		break;
	}

	if (!folder) {
		folder = add_bookmark(NULL, 1, foldername, NULL);
		if (!folder) return;
	} else {
		while (!list_empty(folder->child))
			delete_bookmark(folder->child.next);
	}

	foreachback_tab (tab, term->windows) {
		struct session *ses = tab->data;

		if (!get_current_url(ses, url, MAX_STR_LEN)) {
			if (!ses->loading_uri) continue;
			safe_strncpy(url, struri(ses->loading_uri), MAX_STR_LEN);
		}

		if (!get_current_title(tab->data, title, MAX_STR_LEN)) {
			if (!ses->loading_uri) continue;
			/* TODO: Check globhist. --jonas */
			safe_strncpy(title, struri(ses->loading_uri), MAX_STR_LEN);
		}

		add_bookmark(folder, 1, title, url);
	}
}

void
open_bookmark_folder(struct session *ses, unsigned char *foldername)
{
	struct bookmark *bookmark;
	struct bookmark *folder = NULL;
	struct bookmark *current = NULL;

	assert(foldername && ses);
	if_assert_failed return;

	foreach (bookmark, bookmarks) {
		if (strcmp(bookmark->title, foldername))
			continue;
		folder = bookmark;
		break;
	}

	if (!folder) return;

	foreachback (bookmark, folder->child) {
		struct uri *uri;

		if (bookmark->box_item->type == BI_FOLDER
		    || !*bookmark->url)
			continue;

		/* Save the first bookmark for the current tab */
		if (!current) {
			current = bookmark;
			goto_url(ses, current->url);
			continue;
		}

		uri = get_uri(bookmark->url, -1);
		if (uri) {
			open_url_in_new_tab(ses, uri, 1);
			done_uri(uri);
		}
	}
}
