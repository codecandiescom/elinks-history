/* Internal bookmarks support */
/* $Id: bookmarks.c,v 1.42 2002/09/14 21:21:39 pasky Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "bfu/listbox.h"
#include "bookmarks/bookmarks.h"
#include "lowlevel/home.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"

/* The list of bookmarks */
struct list_head bookmarks = { &bookmarks, &bookmarks };
struct list_head bookmark_box_items = { &bookmark_box_items,
					&bookmark_box_items };
struct list_head bookmark_boxes = { &bookmark_boxes, &bookmark_boxes };

/* Last searched values */
unsigned char *bm_last_searched_name = NULL;
unsigned char *bm_last_searched_url = NULL;


#ifdef BOOKMARKS

/* Set to 1, if bookmarks have changed. */
int bookmarks_dirty = 0;


/* Deletes a bookmark. Returns 0 on failure (no such bm), 1 on success. */
int
delete_bookmark(struct bookmark *bm)
{
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

/* Adds a bookmark to the bookmark list. */
struct bookmark *
add_bookmark(struct bookmark *root, const unsigned char *title,
	     const unsigned char *url)
{
	struct bookmark *bm = mem_alloc(sizeof(struct bookmark));

	if (!bm) return NULL;

	bm->title = stracpy((unsigned char *) title);
	if (!bm->title) {
		free(bm);
		return NULL;
	}

	bm->url = stracpy((unsigned char *) url);
	if (!bm->url) {
		free(bm->title);
		free(bm);
		return NULL;
	}

	bm->root = root;
	init_list(bm->child);

	bm->refcount = 0;

	/* Actually add it */
	/* add_at_pos() is here to add it at the _end_ of the list,
	 * not vice versa. */
	add_at_pos((struct bookmark *) (root ? root->child.prev
					      : bookmarks.prev),
		   bm);
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
	bm->box_item->expanded = 1; /* XXX: Temporary hack. */

	bm->box_item->text = ((unsigned char *) bm->box_item
			      + sizeof(struct listbox_item));
	bm->box_item->box = &bookmark_boxes;
	bm->box_item->udata = (void *) bm;

	strcpy(bm->box_item->text, bm->title);

	add_at_pos((struct listbox_item *) (root ? root->box_item->child.prev
						 : bookmark_box_items.prev),
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

		bm->box_item = mem_realloc(bm->box_item,
					   sizeof(struct listbox_item)
					   + strlen(bm->title) + 1);
		strcpy(bm->box_item->text, bm->title);
	}

	if (url) {
		mem_free(bm->url);
		bm->url = stracpy((unsigned char *) url);
	}
	bookmarks_dirty = 1;

	return 1;
}

void
set_bookmarks_visible(struct list_head bookmark_list,
		      int (*test)(struct bookmark *, void *), void *data)
{
	struct bookmark *bm;

	foreach (bm, bookmark_list) {
		bm->box_item->visible = test(bm, data);
		if (!list_empty(bm->child))
			set_bookmarks_visible(bm->child, test, data);
	}
}

int
test_true(struct bookmark *bm, void *d) {
	return 1;
}

/* Searchs a substring either in title or url fields (ignoring
 * case).  If search_title and search_url are not empty, it selects bookmarks
 * matching the first OR the second.
 *
 * Perhaps another behavior could be to search bookmarks matching both
 * (replacing OR by AND), but it would break a cool feature: when on a page,
 * opening search dialog will have fields corresponding to that page, so
 * pressing ok will find any bookmark with that title or url, permitting a
 * rapid search of an already existing bookmark. --Zas */

struct bookmark_search_ctx {
	unsigned char *search_url;
	unsigned char *search_title;
};

int
test_search(struct bookmark *bm, void *d) {
	struct bookmark_search_ctx *ctx = d;

	return ((ctx->search_title && *ctx->search_title
		 && strcasestr(bm->title, ctx->search_title)) ||
		(ctx->search_url && *ctx->search_url
		 && strcasestr(bm->url, ctx->search_url)));
}

int
bookmark_simple_search(unsigned char *search_url, unsigned char *search_title)
{
	struct bookmark_search_ctx ctx;

	if (!search_title || !search_url)
		return 0;

	/* Memorize last searched title */
	if (bm_last_searched_name) mem_free(bm_last_searched_name);
	bm_last_searched_name = stracpy(search_title);

	/* Memorize last searched url */
	if (bm_last_searched_url) mem_free(bm_last_searched_url);
	bm_last_searched_url = stracpy(search_url);

	if (!*search_title && !*search_url) {
		set_bookmarks_visible(bookmarks, test_true, NULL);
	        return 1;
	}

	ctx.search_url = search_url;
	ctx.search_title = search_title;

	set_bookmarks_visible(bookmarks, test_search, &ctx);

	return 1;
}


/* Loads the bookmarks from file */
void
read_bookmarks()
{
	/* INBUF_SIZE = max. title length + 1 byte for separator
	 * + max. url length + 1 byte for separator + 4 bytes for depth
	 * + 1 byte for end of line + 1 byte for null char + reserve */
#define INBUF_SIZE ((MAX_STR_LEN - 1) + 1 + (MAX_STR_LEN - 1) + 1 + 4 + 1 + 1 \
		    + MAX_STR_LEN)
	unsigned char in_buffer[INBUF_SIZE]; /* read buffer */
	unsigned char *file_name;
	FILE *f;
	struct bookmark *last_bm = NULL;
	int last_depth = 0;

	file_name = straconcat(elinks_home, "bookmarks", NULL);
	if (!file_name) return;

	f = fopen(file_name, "r");
	mem_free(file_name);
	if (!f) return;

	/* TODO: Ignore lines with bad chars in title or url (?). -- Zas */
	while (fgets(in_buffer, INBUF_SIZE, f)) {
		unsigned char *title = in_buffer;
		unsigned char *url;
		unsigned char *depth_str;
		int depth;
		unsigned char *line_end;

		/* Load URL. */

		url = strchr(in_buffer, '\t');

		/* If separator is not found, or title is empty or too long,
		 * skip that line. */
		if (!url || url == in_buffer
		    || url - in_buffer > MAX_STR_LEN - 1)
			continue;
		*url = '\0';
		url++;

		/* Load depth. */

		depth_str = strchr(url, '\t');

		if (depth_str && (depth_str - url > MAX_STR_LEN - 1
				  || depth_str == url))
			continue;

		if (!depth_str) {
			depth_str = url;
			depth = 0;
		} else {
			*depth_str = '\0';
			depth_str++;
			depth = atoi(depth_str);
			if (depth < 0) depth = 0;
			if (depth > last_depth + 1) depth = last_depth + 1;
			if (!last_bm && depth > 0) depth = 0;
		}

		/* Load EOLN. */

		line_end = strchr(depth_str, '\n');
		if (!line_end)
			continue;
		*line_end = '\0';

		{
			struct bookmark *root = NULL;

			if (depth > 0) {
				if (depth == last_depth) {
					root = last_bm->root;
				} else if (depth > last_depth) {
					root = last_bm;
				} else {
					while (last_depth - depth) {
						last_bm = last_bm->root;
						last_depth--;
					}
					root = last_bm;
				}
			}
			last_bm = add_bookmark(root, title, url);
			last_depth = depth;
		}
	}

	fclose(f);
	bookmarks_dirty = 0;
#undef INBUF_SIZE
}

/* Saves the bookmarks to file */
void
write_bookmarks_do(struct secure_save_info *ssi, struct list_head *bookmarks)
{
	struct bookmark *bm;

	foreach (bm, *bookmarks) {
		unsigned char depth[16];
		unsigned char *p = stracpy(bm->title);
		int i;

		for (i = strlen(p) - 1; i >= 0; i--)
			if (p[i] < ' ')
				p[i] = ' ';
		secure_fputs(ssi, p);
		secure_fputc(ssi, '\t');
		secure_fputs(ssi, bm->url);
		secure_fputc(ssi, '\t');
		snprintf(depth, 16, "%d", bm->box_item->depth);
		secure_fputs(ssi, depth);
		secure_fputc(ssi, '\n');
		mem_free(p);
		if (ssi->err) break;

		if (!list_empty(bm->child))
			write_bookmarks_do(ssi, &bm->child);
	}
}

void
write_bookmarks()
{
	struct secure_save_info *ssi;
	unsigned char *file_name;

	if (!bookmarks_dirty) return;

	file_name = straconcat(elinks_home, "bookmarks", NULL);
	if (!file_name) return;

	ssi = secure_open(file_name, 0177);
	mem_free(file_name);
	if (!ssi) return;

	write_bookmarks_do(ssi, &bookmarks);

	if (!secure_close(ssi)) bookmarks_dirty = 0;
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
