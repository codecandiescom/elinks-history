/* Internal bookmarks support */
/* $Id: bookmarks.c,v 1.11 2002/04/01 22:20:00 pasky Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <links.h>

#include <bookmarks/bookmarks.h>
#include <config/default.h>

/* Whether to save bookmarks after each modification of their list
 * (add/modify/delete). */
#define BOOKMARKS_RESAVE	1


/* The list of bookmarks */
struct list_head bookmarks = { &bookmarks, &bookmarks };

/* The last used id of a bookmark */
bookmark_id next_bookmark_id = 0;

/* search memorization */
unsigned char *bm_last_searched_name = NULL;
unsigned char *bm_last_searched_url = NULL;

static void write_bookmarks();


/* Gets a bookmark by id */
struct bookmark *
get_bookmark_by_id(bookmark_id id)
{
	struct bookmark *bm;

	if (id == BAD_BOOKMARK_ID)
		return NULL;

	foreach (bm, bookmarks) {
		if (id == bm->id)
			return bm;
	}

	return NULL;
}

/* Deletes a bookmark, given the id. Returns 0 on failure (no such bm), 1 on
 * success */
int
delete_bookmark_by_id(bookmark_id id)
{
	struct bookmark *bm = get_bookmark_by_id(id);

	if (!bm) return 0;

	del_from_list(bm);

	/* Now wipe the bookmark */
	mem_free(bm->title);
	mem_free(bm->url);
	mem_free(bm);

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif

	return 1;
}

/* Adds a bookmark to the bookmark list. Don't play with new_bm after you're
 * done. It would be impolite. */
void
add_bookmark(const unsigned char *title, const unsigned char *url)
{
	struct bookmark *bm = mem_alloc(sizeof(struct bookmark));

	if (!bm) return;

	bm->title = stracpy((unsigned char *) title);
	if (!bm->title) {
		free(bm);
		return;
	}

	bm->url = stracpy((unsigned char *) url);
	if (!bm->url) {
		free(bm->title);
		free(bm);
		return;
	}

	bm->id = next_bookmark_id++;
	bm->selected = 1;

	/* Actually add it */
	add_to_list(bookmarks, bm);

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}

/* Updates an existing bookmark.
 *
 * If the requested bookmark does not exist, return 0. Otherwise, return 1.
 *
 * If any of the fields are NULL, the value is left unchanged. */
int
update_bookmark(bookmark_id id, const unsigned char *title,
		const unsigned char *url)
{
	struct bookmark *bm = get_bookmark_by_id(id);

	if (!bm) {
		/* Does not exist. */
		return 0;
	}

	if (title) {
		mem_free(bm->title);
		bm->title = stracpy((unsigned char *)title);
	}

	if (url) {
		mem_free(bm->url);
		bm->url = stracpy((unsigned char *)url);
	}

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif

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

	if (!search_title || !search_url) return 0;

	/* memorize title criteria */
	if (bm_last_searched_name) mem_free(bm_last_searched_name);
	bm_last_searched_name = stracpy(search_title);

	/* memorize url criteria */
	if (bm_last_searched_url) mem_free(bm_last_searched_url);
	bm_last_searched_url = stracpy(search_url);

	if (!*search_title && !*search_url) {
		foreach(bm, bookmarks) {
			bm->selected = 1;
		}
	        return 1;
	}

	foreach(bm, bookmarks) {
		bm->selected = 0;
		if ((search_title && *search_title
		     && strcasestr(bm->title, search_title)) ||
		    (search_url && *search_url
		     && strcasestr(bm->url, search_url)))
			bm->selected = 1;
	}
	return 1;
}


/* Loads the bookmarks from file */
void
read_bookmarks()
{
	unsigned char in_buffer[MAX_STR_LEN]; /* read buffer */
	unsigned char *file_name;
	unsigned char *title;	/* Pointer to the start of title in buffer */
	unsigned char *url;	/* Pointer to the start of url in buffer */
	FILE *f;

	file_name = stracpy(links_home);
	if (!file_name) return;
	add_to_strn(&file_name, "bookmarks");

	f = fopen(file_name, "r");
	mem_free(file_name);
	if (!f)	return;

	title = in_buffer;

	/* FIXME: very long lines aren't handled !!! --Zas */
	/* TODO: Use rather \t as a separator. */
	while (fgets(in_buffer, MAX_STR_LEN, f)) {
		unsigned char *urlend;

		url = strchr(in_buffer, '|');
		if (!url) continue;
		*url = '\0';
		url++;
		urlend = strchr(url, '\n');
		if (urlend) *urlend = '\0';
		add_bookmark(title, url);
	}

	fclose(f);
}

/* Saves the bookmarks to file */
static void
write_bookmarks()
{
	struct bookmark *bm;
	FILE *out;
	unsigned char *file_name;

	file_name = stracpy(links_home);
	if (!file_name) return;
	add_to_strn(&file_name, "bookmarks");

	out = fopen(file_name, "w");
	mem_free(file_name);
	if (!out) return;

	/* TODO: Use rather \t as a separator. */
	foreachback(bm, bookmarks) {
		unsigned char *p = stracpy(bm->title);
		int i;

		for (i = strlen(p) - 1; i >= 0; i--)
			if (p[i] < ' '|| p[i] == '|')
				p[i] = '*';
		fputs(p,out);
		fputc('|', out);
		fputs(bm->url,out);
		fputc('\n',out);
		mem_free(p);
	}

	fclose(out);
}

/* Clears the bookmark list */
void
free_bookmarks()
{
	struct bookmark *bm;

	foreach (bm, bookmarks) {
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
