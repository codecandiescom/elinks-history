/* Internal bookmarks support */
/* $Id: bookmarks.c,v 1.145 2004/12/14 17:08:56 miciah Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bookmarks/backend/common.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "protocol/uri.h"
#include "sched/task.h"
#include "terminal/tab.h"
#include "util/conv.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/secsave.h"
#include "util/string.h"

/* The list of bookmarks */
INIT_LIST_HEAD(bookmarks);

/* Set to 1, if bookmarks have changed. */
static int bookmarks_dirty = 0;

static struct hash *bookmark_cache = NULL;



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
	if (bookmark_cache) {
		free_hash(bookmark_cache);
		bookmark_cache = NULL;
	}
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
	if (get_cmd_opt_bool("anonymous")) {
		bookmarks_unset_dirty();
		return;
	}
	bookmarks_write(&bookmarks);
}




/* Bookmarks manipulation */

void
bookmarks_set_dirty(void)
{
	bookmarks_dirty = 1;
}

void
bookmarks_unset_dirty(void)
{
	bookmarks_dirty = 0;
}

int
bookmarks_are_dirty(void)
{
	return (bookmarks_dirty == 1);
}

#define check_bookmark_cache(url) (bookmark_cache && (url) && *(url))

/* Deletes a bookmark. Returns 0 on failure (no such bm), 1 on success. */
void
delete_bookmark(struct bookmark *bm)
{
	while (!list_empty(bm->child)) {
		delete_bookmark(bm->child.next);
	}

	if (check_bookmark_cache(bm->url)) {
		struct hash_item *item;

		item = get_hash_item(bookmark_cache, bm->url, strlen(bm->url));
		if (item) del_hash_item(bookmark_cache, item);
	}

	del_from_list(bm);
	bookmarks_set_dirty();

	/* Now wipe the bookmark */
	done_listbox_item(&bookmark_browser, bm->box_item);

	mem_free(bm->title);
	mem_free(bm->url);
	mem_free(bm);
}

/* Deletes any bookmarks with no URLs (i.e., folders) and of which
 * the title matches the given argument. */
static void
delete_folder_by_name(unsigned char *foldername)
{
	struct bookmark *bookmark, *next;

	foreachsafe (bookmark, next, bookmarks) {
		if ((bookmark->url && *bookmark->url)
		    || strcmp(bookmark->title, foldername))
			continue;

		delete_bookmark(bookmark);
	}
}

/* Adds a bookmark to the bookmark list. Place 0 means top, place 1 means
 * bottom. NULL or "" @url means it is a bookmark folder. */
struct bookmark *
add_bookmark(struct bookmark *root, int place, unsigned char *title,
	     unsigned char *url)
{
	struct bookmark *bm;

	bm = mem_calloc(1, sizeof(struct bookmark));
	if (!bm) return NULL;

	bm->title = stracpy(title);
	if (!bm->title) {
		mem_free(bm);
		return NULL;
	}
	sanitize_title(bm->title);

	bm->url = stracpy(empty_string_or_(url));
	if (!bm->url || !sanitize_url(bm->url)) {
		mem_free_if(bm->url);
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
	bookmarks_set_dirty();

	/* Setup box_item */
	/* Note that item_free is left at zero */
	bm->box_item = mem_calloc(1, sizeof(struct listbox_item));
	if (!bm->box_item) return NULL;
	if (root) {
		bm->box_item->depth = root->box_item->depth + 1;
	}
	init_list(bm->box_item->child);
	bm->box_item->visible = 1;

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

	/* Hash creation if needed. */
	if (!bookmark_cache)
		bookmark_cache = init_hash(8, &strhash);

	/* Create a new entry. */
	if (check_bookmark_cache(bm->url))
		add_hash_item(bookmark_cache, bm->url, strlen(bm->url), bm);

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
	}

	if (url2) {
		if (check_bookmark_cache(bm->url)) {
			struct hash_item *item;
			int len = strlen(bm->url);

			item = get_hash_item(bookmark_cache, bm->url, len);
			if (item) del_hash_item(bookmark_cache, item);
		}

		if (check_bookmark_cache(url2)) {
			add_hash_item(bookmark_cache, url2, strlen(url2), bm);
		}

		mem_free(bm->url);
		bm->url = url2;
	}

	bookmarks_set_dirty();

	return 1;
}

/* Search bookmark cache for item matching url. */
struct bookmark *
get_bookmark(unsigned char *url)
{
	struct hash_item *item;

	if (!check_bookmark_cache(url))
		return NULL;

	/* Search for cached entry. */

	item = get_hash_item(bookmark_cache, url, strlen(url));

	return item ? item->value : NULL;
}

void
bookmark_terminal_tabs(struct terminal *term, unsigned char *foldername)
{
	unsigned char title[MAX_STR_LEN], url[MAX_STR_LEN];
	struct bookmark *folder = add_bookmark(NULL, 1, foldername, NULL);
	struct window *tab;

	if (!folder) return;

	foreachback_tab (tab, term->windows) {
		struct session *ses = tab->data;

		if (!get_current_url(ses, url, MAX_STR_LEN))
			continue;

		if (!get_current_title(ses, title, MAX_STR_LEN))
			continue;

		add_bookmark(folder, 1, title, url);
	}
}

void
bookmark_auto_save_tabs(struct terminal *term)
{
	unsigned char *foldername;

	if (get_cmd_opt_bool("anonymous")
	    || !get_opt_bool("ui.sessions.auto_save"))
		return;

	foldername = get_opt_str("ui.sessions.auto_save_foldername");
	if (!*foldername) return;

	/* Ensure uniqueness of the auto save folder, so it is possible to
	 * restore the (correct) session when starting up. */
	delete_folder_by_name(foldername);

	bookmark_terminal_tabs(term, foldername);
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

	foreach (bookmark, folder->child) {
		struct uri *uri;

		if (bookmark->box_item->type == BI_FOLDER
		    || !*bookmark->url)
			continue;

		uri = get_uri(bookmark->url, 0);
		if (!uri) continue;

		/* Save the first bookmark for the current tab */
		if (!current) {
			current = bookmark;
			goto_uri(ses, uri);
		} else {
			open_uri_in_new_tab(ses, uri, 1, 0);
		}

		done_uri(uri);
	}
}
