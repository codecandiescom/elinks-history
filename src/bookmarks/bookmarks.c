/* Internal bookmarks support */
/* $Id: bookmarks.c,v 1.36 2002/08/30 10:58:28 pasky Exp $ */

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
add_bookmark(const unsigned char *title, const unsigned char *url)
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

	/* Actually add it */
	add_to_list(bookmarks, bm);
	bookmarks_dirty = 1;

	/* Setup box_item */
	/* Note that item_free is left at zero */

	bm->box_item = mem_calloc(1, sizeof(struct listbox_item)
				     + strlen(bm->title) + 1);
	if (!bm->box_item) return NULL;
	init_list(bm->box_item->child);
	bm->box_item->visible = 1;

	bm->box_item->text = ((unsigned char *) bm->box_item
			      + sizeof(struct listbox_item));
	if (!list_empty(bookmark_box_items))
		bm->box_item->box = ((struct listbox_item *)
				     bookmark_box_items.next)->box;
	bm->box_item->udata = (void *) bm;

	strcpy(bm->box_item->text, bm->title);

	add_to_list(bookmark_box_items, bm->box_item);

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

/* Searchs a substring either in title or url fields (ignoring
 * case).  If search_title and search_url are not empty, it selects bookmarks
 * matching the first OR the second.
 *
 * Perhaps another behavior could be to search bookmarks matching both
 * (replacing OR by AND), but it would break a cool feature: when on a page,
 * opening search dialog will have fields corresponding to that page, so
 * pressing ok will find any bookmark with that title or url, permitting a
 * rapid search of an already existing bookmark. --Zas */
int
bookmark_simple_search(unsigned char *search_url, unsigned char *search_title)
{
	struct bookmark *bm;

	if (!search_title || !search_url)
		return 0;

	/* Memorize last searched title */
	if (bm_last_searched_name) mem_free(bm_last_searched_name);
	bm_last_searched_name = stracpy(search_title);

	/* Memorize last searched url */
	if (bm_last_searched_url) mem_free(bm_last_searched_url);
	bm_last_searched_url = stracpy(search_url);

	if (!*search_title && !*search_url) {
		foreach (bm, bookmarks) {
			bm->box_item->visible = 1;
		}
	        return 1;
	}

	foreach (bm, bookmarks) {
		if ((search_title && *search_title
		     && strcasestr(bm->title, search_title)) ||
		    (search_url && *search_url
		     && strcasestr(bm->url, search_url))) {
			bm->box_item->visible = 1;
		} else {
			bm->box_item->visible = 0;
		}
	}
	return 1;
}


/* Loads the bookmarks from file */
void
read_bookmarks()
{
	/* INBUF_SIZE = max. title length + 1 byte for separator
	 * + max. url length + 1 byte for end of line + 1 byte for null char */
#define INBUF_SIZE (MAX_STR_LEN - 1) + 1 + (MAX_STR_LEN - 1) + 1 + 1
	unsigned char in_buffer[INBUF_SIZE]; /* read buffer */
	unsigned char *file_name;
	unsigned char *title;	/* Pointer to the start of title in buffer */
	unsigned char *url;	/* Pointer to the start of url in buffer */
	FILE *f;

	file_name = straconcat(elinks_home, "bookmarks", NULL);
	if (!file_name) return;

	f = fopen(file_name, "r");
	mem_free(file_name);
	if (!f) return;

	title = in_buffer;

	/* TODO: Ignore lines with bad chars in title or url (?). -- Zas */
	while (fgets(in_buffer, INBUF_SIZE, f)) {
		unsigned char *urlend;

		url = strchr(in_buffer, '\t');

		/* If separator is not found, or title is empty or too long,
		 * skip that line -- Zas */
		if (!url || url == in_buffer
		    || url - in_buffer > MAX_STR_LEN - 1)
			continue;
		*url = '\0';

		/* Move to start of url */
		url++;

		urlend = strchr(url, '\n');
		/* If end of line is not found, or url is empty or too long,
		 * skip that line -- Zas */
		if (!urlend || url == urlend || urlend - url > MAX_STR_LEN - 1)
			continue;
		*urlend = '\0';

		add_bookmark(title, url);
	}

	fclose(f);
	bookmarks_dirty = 0;
#undef INBUF_SIZE
}

/* Saves the bookmarks to file */
void
write_bookmarks()
{
	struct bookmark *bm;
	struct secure_save_info *ssi;
	unsigned char *file_name;

	if (!bookmarks_dirty) return;

	file_name = straconcat(elinks_home, "bookmarks", NULL);
	if (!file_name) return;

	ssi = secure_open(file_name, 0177);
	mem_free(file_name);
	if (!ssi) return;

	foreachback(bm, bookmarks) {
		unsigned char *p = stracpy(bm->title);
		int i;

		for (i = strlen(p) - 1; i >= 0; i--)
			if (p[i] < ' ')
				p[i] = ' ';
		secure_fputs(ssi, p);
		secure_fputc(ssi, '\t');
		secure_fputs(ssi, bm->url);
		secure_fputc(ssi, '\n');
		mem_free(p);
		if (ssi->err) break;
	}

	if (!secure_close(ssi)) bookmarks_dirty = 0;
}

/* Clears the bookmark list */
void
free_bookmarks()
{
	struct bookmark *bm;

	foreach (bm, bookmarks) {
		del_from_list(bm->box_item);
		mem_free(bm->box_item);
		mem_free(bm->title);
		mem_free(bm->url);
	}

	free_list(bookmarks);
}

/* Does final cleanup and saving of bookmarks */
void
finalize_bookmarks()
{
	write_bookmarks();
	free_bookmarks();
	if (bm_last_searched_name) mem_free(bm_last_searched_name);
	if (bm_last_searched_url) mem_free(bm_last_searched_url);
}

#else /* BOOKMARKS */

void read_bookmarks() {}
void write_bookmarks() {}
void finalize_bookmarks() {}
int delete_bookmark(struct bookmark *bm) { return 0; }
void add_bookmark(const unsigned char *u, const unsigned char *t) {}
int update_bookmark(struct bookmark *bm, const unsigned char *u, const unsigned char *t) { return 0; }
int bookmark_simple_search(unsigned char *u, unsigned char *t){ return 0; }
#endif
