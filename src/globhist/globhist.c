/* Global history */
/* $Id: globhist.c,v 1.32 2003/07/15 22:18:02 miciah Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "bfu/listbox.h"
#include "config/options.h"
#include "globhist/dialogs.h"
#include "globhist/globhist.h"
#include "lowlevel/home.h"
#include "lowlevel/select.h"
#include "lowlevel/ttime.h"
#include "util/file.h"
#include "util/hash.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"
#include "util/lists.h"


struct global_history_list global_history = {
	0,
	{ D_LIST_HEAD(global_history.items) }
};

INIT_LIST_HEAD(gh_box_items);
INIT_LIST_HEAD(gh_boxes);


/* GUI stuff. Declared here because finalize_global_history() frees it. */
unsigned char *gh_last_searched_title = NULL;
unsigned char *gh_last_searched_url = NULL;

/* Timer for periodically writing the history to disk. */
static int global_history_write_timer = -1;

#ifdef GLOBHIST

struct globhist_cache_entry {
	LIST_HEAD(struct globhist_cache_entry);

	struct global_history_item *item;
};

static struct hash *globhist_cache = NULL;
static int globhist_cache_entries = 0;
static int globhist_dirty = 0;
static int globhist_nosave = 0;


static void
free_global_history_item(struct global_history_item *historyitem)
{
	if (globhist_cache) {
		struct hash_item *item;

		item = get_hash_item(globhist_cache, historyitem->url, strlen(historyitem->url));
		if (item) {
			mem_free(item->value);
			del_hash_item(globhist_cache, item);
			globhist_cache_entries--;
		}
	}

	del_from_list(historyitem->box_item);
	mem_free(historyitem->box_item);

	mem_free(historyitem->title);
	mem_free(historyitem->url);
}

void
delete_global_history_item(struct global_history_item *historyitem)
{
	struct listbox_item *item = historyitem->box_item;
	struct listbox_data *box;

	/* If this happens inside of the box, move top/sel if needed. */

	foreach (box, *item->box) {

	/* Please see relevant parts of bookmarks. */

	if (box) {
		if (box->sel && item == box->sel) {
			box->sel = traverse_listbox_items_list(item, -1,
					1, NULL, NULL);
			if (item == box->sel)
				box->sel = traverse_listbox_items_list(item, 1,
						1, NULL, NULL);
			if (item == box->sel)
				box->sel = NULL;
		}

		if (box->top && item == box->top) {
			box->top = traverse_listbox_items_list(item, 1,
					1, NULL, NULL);
			if (item == box->top)
				box->top = traverse_listbox_items_list(item, -1,
						1, NULL, NULL);
			if (item == box->top)
				box->top = NULL;
		}
	}

	}

	free_global_history_item(historyitem);
	del_from_list(historyitem);
	mem_free(historyitem);
	global_history.n--;

	update_all_history_dialogs();
	globhist_dirty = 1;
}

/* Search global history for item matching url. */
struct global_history_item *
get_global_history_item(unsigned char *url)
{
	struct hash_item *item;

	if (!url) return NULL;
	if (!globhist_cache) return NULL;

	/* Search for cached entry. */

	item = get_hash_item(globhist_cache, url, strlen(url));
	if (!item) return NULL;
	return ((struct globhist_cache_entry *) item->value)->item;
}

#if 0
/* Search global history for certain item. There must be full match with the
 * parameter or the parameter must be NULL/zero. */
struct global_history_item *
multiget_global_history_item(unsigned char *url, unsigned char *title, ttime time)
{
	struct global_history_item *historyitem;

	/* Code duplication vs performance, since this function is called most
	 * of time for url matching only... Execution time is divided by 2. */
	if (url && !title && !time) {
		return get_global_history_item(url);
	} else {
		foreach (historyitem, global_history.items) {
			if ((!url || !strcmp(historyitem->url, url)) &&
			    (!title || !strcmp(historyitem->title, title)) &&
			    (!time || historyitem->last_visit == time)) {
				return historyitem;
			}
		}
	}

	return NULL;
}
#endif

static void
free_globhist_cache(void)
{
	if (globhist_cache) {
		struct hash_item *item;
		int i;

		foreach_hash_item (item, *globhist_cache, i)
			if (item->value)
				mem_free(item->value);

		free_hash(globhist_cache);
	}

	globhist_cache = NULL;
	globhist_cache_entries = 0;
}

/* Add a new entry in history list, take care of duplicate, respect history
 * size limit, and update any open history dialogs. */
void
add_global_history_item(unsigned char *url, unsigned char *title, ttime vtime)
{
	struct global_history_item *history_item;
	unsigned char *text;
	int max_globhist_items;

	if (!get_opt_bool("document.history.global.enable"))
		return;

	if (!url)
		return;

	max_globhist_items = get_opt_int("document.history.global.max_items");

	history_item = get_global_history_item(url);
	if (history_item) delete_global_history_item(history_item);

	while (global_history.n >= max_globhist_items) {
		history_item = global_history.items.prev;

		if ((void *) history_item == &global_history.items) {
			internal("global history is empty");
			global_history.n = 0;
			return;
		}

		delete_global_history_item(history_item);
	}

	history_item = mem_alloc(sizeof(struct global_history_item));
	if (!history_item)
		return;

	history_item->last_visit = vtime;
	history_item->title = stracpy(title ? title : (unsigned char *) "");
	if (!history_item->title) {
		mem_free(history_item);
		return;
	}
	history_item->url = stracpy(url);
	if (!history_item->url) {
		mem_free(history_item->title);
		mem_free(history_item);
		return;
	}
	history_item->refcount = 0;

	add_to_list(global_history.items, history_item);
	global_history.n++;

	text = get_opt_int("document.history.global.display_type")
		? history_item->title : history_item->url;
	if (!*text) text = history_item->url;

	/* Deleted in history_dialog_clear_list() */
	history_item->box_item = mem_calloc(1, sizeof(struct listbox_item)
					       + strlen(text) + 1);
	if (!history_item->box_item) return;
	init_list(history_item->box_item->child);
	history_item->box_item->visible = 1;

	history_item->box_item->text = ((unsigned char *) history_item->box_item
					+ sizeof(struct listbox_item));
	history_item->box_item->box = &gh_boxes;
	history_item->box_item->udata = (void *) history_item;

	strcpy(history_item->box_item->text, text);

	add_to_list(gh_box_items, history_item->box_item);

	update_all_history_dialogs();

	if (!globhist_nosave) globhist_dirty = 1;

	/* Hash creation if needed. */
	if (!globhist_cache)
		globhist_cache = init_hash(8, &strhash);

	if (globhist_cache && globhist_cache_entries < max_globhist_items) {
		/* Create a new entry. */
		struct globhist_cache_entry *globhistce = mem_alloc(sizeof(struct globhist_cache_entry));

		if (!globhistce) return;

		globhistce->item = history_item;
		if (!add_hash_item(globhist_cache, history_item->url, strlen(history_item->url), globhistce)) {
			mem_free(globhistce);
		} else {
			globhist_cache_entries++;
		}
	}
}


int
globhist_simple_search(unsigned char *search_url, unsigned char *search_title)
{
	struct global_history_item *item;

	if (!search_title || !search_url)
		return 0;

	/* Memorize last searched title */
	if (gh_last_searched_title) mem_free(gh_last_searched_title);
	gh_last_searched_title = stracpy(search_title);
	if (!gh_last_searched_title) return 0;

	/* Memorize last searched url */
	if (gh_last_searched_url) mem_free(gh_last_searched_url);
	gh_last_searched_url = stracpy(search_url);
	if (!gh_last_searched_url) {
		mem_free(gh_last_searched_title);
		return 0;
	}

	if (!*search_title && !*search_url) {
		foreach (item, global_history.items) {
			item->box_item->visible = 1;
		}
		return 1;
	}

	foreach (item, global_history.items) {
		if ((search_title && *search_title
		     && strcasestr(item->title, search_title)) ||
		    (search_url && *search_url
		     && strcasestr(item->url, search_url))) {
			item->box_item->visible = 1;
		} else {
			item->box_item->visible = 0;
		}
	}
	return 1;
}


void
static read_global_history(void)
{
	unsigned char in_buffer[MAX_STR_LEN];
	unsigned char *file_name = "globhist";
	unsigned char *title, *url, *last_visit;
	FILE *f;

	if (!get_opt_bool("document.history.global.enable"))
		return;

	if (elinks_home) {
		file_name = straconcat(elinks_home, file_name, NULL);
		if (!file_name) return;
	}
	f = fopen(file_name, "r");
	if (elinks_home) mem_free(file_name);
	if (!f) return;

	title = in_buffer;
	globhist_nosave = 1;

	while (safe_fgets(in_buffer, MAX_STR_LEN, f)) {
		/* Drop ending '\n'. */
		if (*in_buffer) in_buffer[strlen(in_buffer) - 1] = 0;

		url = strchr(in_buffer, '\t');
		if (!url)
			continue;
		*url = '\0';
		url++; /* Now url points to the character after \t. */

		last_visit = strchr(url, '\t');
		if (!last_visit)
			continue;
		*last_visit = '\0';
		last_visit++;

		/* Is using atol() in this way acceptable? It seems
		 * non-portable to me; ttime might not be a long. -- Miciah */
		add_global_history_item(url, title, atol(last_visit));
	}

	fclose(f);
	globhist_nosave = 0;
}

static void
write_global_history(void)
{
	struct global_history_item *historyitem;
	unsigned char *file_name;
	struct secure_save_info *ssi;

	if (!globhist_dirty || !elinks_home
	    || !get_opt_bool("document.history.global.enable"))
		return;

	file_name = straconcat(elinks_home, "globhist", NULL);
	if (!file_name) return;

	ssi = secure_open(file_name, 0177); /* rw for user only */
	mem_free(file_name);
	if (!ssi) return;

	foreachback (historyitem, global_history.items) {
		unsigned char *p;
		int i;
		int bad = 0;

		p = historyitem->title;
		for (i = strlen(p) - 1; i >= 0; i--)
			if (p[i] < ' ')
				p[i] = ' ';

		p = historyitem->url;
		for (i = strlen(p) - 1; i >= 0; i--)
			if (p[i] < ' ')
				bad = 1; /* Incorrect url, skip it. */

		if (bad) continue;

		if (secure_fprintf(ssi, "%s\t%s\t%ld\n",
				   historyitem->title,
				   historyitem->url,
				   historyitem->last_visit) < 0) break;
	}

	secure_close(ssi);

	globhist_dirty = 0;
}

static void
free_global_history(void)
{
	struct global_history_item *historyitem;

	free_globhist_cache();

	foreach (historyitem, global_history.items) {
		free_global_history_item(historyitem);
	}
	free_list(global_history.items);
}

static void
global_history_write_timer_handler(void *xxx)
{
	int interval = get_opt_int("document.history.global.write_interval");

	write_global_history();

	if (!interval) return;

	global_history_write_timer =
		install_timer(interval * 1000,
			      global_history_write_timer_handler,
			      NULL);
}

int
global_history_write_timer_change_hook(struct session *ses,
				       struct option *current,
				       struct option *changed)
{
	if (global_history_write_timer >= 0) {
		kill_timer(global_history_write_timer);
		global_history_write_timer = -1;
	}

	if (elinks_home && !get_opt_int_tree(&cmdline_options, "anonymous"))
		global_history_write_timer_handler(NULL);

	return 0;
}

void
init_global_history(void)
{
	read_global_history();

	if (elinks_home && !get_opt_int_tree(&cmdline_options, "anonymous"))
		global_history_write_timer_handler(NULL);
}

void
finalize_global_history(void)
{
	if (global_history_write_timer >= 0)
		kill_timer(global_history_write_timer);
	write_global_history();
	free_global_history();
	if (gh_last_searched_title) mem_free(gh_last_searched_title);
	if (gh_last_searched_url) mem_free(gh_last_searched_url);
}

#endif /* GLOBHIST */
