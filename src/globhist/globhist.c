/* Global history */
/* $Id: globhist.c,v 1.3 2002/08/30 22:55:28 pasky Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "links.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bfu/listbox.h"
#include "config/options.h"
#include "globhist/dialogs.h"
#include "globhist/globhist.h"
#include "lowlevel/home.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"
#include "util/lists.h"


struct global_history_list global_history = {
	0,
	{ &global_history.items, &global_history.items }
};

struct list_head gh_box_items = { &gh_box_items, &gh_box_items };

struct list_head gh_boxes = { &gh_boxes, &gh_boxes };

/* GUI stuff. Declared here because finalize_global_history() frees it. */
unsigned char *gh_last_searched_title = NULL;
unsigned char *gh_last_searched_url = NULL;


#ifdef GLOBHIST


void
free_global_history_item(struct global_history_item *historyitem)
{
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
}

/* Search global history for certain item. There must be full match with the
 * parameter or the parameter must be NULL/zero. */
struct global_history_item *
get_global_history_item(unsigned char *url, unsigned char *title, time_t time)
{
	struct global_history_item *historyitem;

	foreach (historyitem, global_history.items) {
		if ((!url || !strcmp(historyitem->url, url)) &&
		    (!title || !strcmp(historyitem->title, title)) &&
		    (!time || historyitem->last_visit == time)) {
			return historyitem;
		}
	}

	return NULL;
}


/* Add a new entry in history list, take care of duplicate, respect history
 * size limit, and update any open history dialogs. */
void
add_global_history_item(unsigned char *url, unsigned char *title, time_t time)
{
	struct global_history_item *history_item;

	if (!get_opt_int("document.history.global.enable"))
		return;

	if (!title || !url)
		return;

	foreach (history_item, global_history.items) {
		if (!strcmp(history_item->url, url)) {
			delete_global_history_item(history_item);
			break;
		}
	}

	history_item = mem_alloc(sizeof(struct global_history_item));
	if (!history_item)
		return;

	history_item->last_visit = time;
	history_item->title = stracpy(title);
	history_item->url = stracpy(url);

	add_to_list(global_history.items, history_item);
	global_history.n++;

	while (global_history.n > get_opt_int("document.history.global.max_items")) {
		history_item = global_history.items.prev;

		if ((void *) history_item == &global_history.items) {
			internal("global history is empty");
			global_history.n = 0;
			return;
		}

		delete_global_history_item(history_item);
	}

	/* Deleted in history_dialog_clear_list() */
	history_item->box_item = mem_calloc(1, sizeof(struct listbox_item)
					       + strlen(history_item->url) + 1);
	if (!history_item) return;
	init_list(history_item->box_item->child);
	history_item->box_item->visible = 1;

	history_item->box_item->text = ((unsigned char *) history_item->box_item
					+ sizeof(struct listbox_item));
	history_item->box_item->box = &gh_boxes;
	history_item->box_item->udata = (void *) history_item;

	strcpy(history_item->box_item->text, history_item->url);

	add_to_list(gh_box_items, history_item->box_item);

	update_all_history_dialogs();
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

	/* Memorize last searched url */
	if (gh_last_searched_url) mem_free(gh_last_searched_url);
	gh_last_searched_url = stracpy(search_url);

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
read_global_history()
{
	unsigned char in_buffer[MAX_STR_LEN];
	unsigned char *file_name;
	unsigned char *title, *url, *last_visit;
	FILE *f;

	if (!get_opt_int("document.history.global.enable"))
		return;

	file_name = straconcat(elinks_home, "globhist", NULL);
	if (!file_name) return;

	f = fopen(file_name, "r");
	mem_free(file_name);
	if (f == NULL)
		return;

	title = in_buffer;

	while (fgets(in_buffer, MAX_STR_LEN, f)) {
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
		 * non-portable to me; time_t might not be a long. -- Miciah */
		add_global_history_item(url, title, atol(last_visit));
	}

	fclose(f);
}

void
write_global_history()
{
	struct global_history_item *historyitem;
	unsigned char *file_name;
	struct secure_save_info *ssi;

	if (!get_opt_int("document.history.global.enable"))
		return;

	file_name = straconcat(elinks_home, "globhist", NULL);
	if (!file_name) return;

	ssi = secure_open(file_name, 0177); /* rw for user only */
	mem_free(file_name);
	if (!ssi) return;

	foreachback (historyitem, global_history.items) {
		unsigned char *p;
		int i;

		p = historyitem->title;
		for (i = strlen(p) - 1; i >= 0; i--)
			if (p[i] < ' ' || p[i] == '\t')
				p[i] = ' ';

		p = historyitem->url;
		for (i = strlen(p) - 1; i >= 0; i--)
			if (p[i] < ' ' || p[i] == '\t') {
				/* Or maybe skip this URL instead as it will
				 * now be (and was already) incorrect?
				 * -- Miciah */
				p[i] = ' ';
			}

		if (secure_fprintf(ssi, "%s\t%s\t%ld\n",
				   historyitem->title,
				   historyitem->url,
				   historyitem->last_visit) < 0) break;
	}

	secure_close(ssi);
}

static void
free_global_history()
{
	struct global_history_item *historyitem;

	foreach (historyitem, global_history.items) {
		free_global_history_item(historyitem);
	}

	free_list(global_history.items);
}

void
finalize_global_history()
{
	write_global_history();
	free_global_history();
	if (gh_last_searched_title) mem_free(gh_last_searched_title);
	if (gh_last_searched_url) mem_free(gh_last_searched_url);
}

#else /* GLOBHIST */

void read_global_history() {}
void write_global_history() {}
void finalize_global_history() {}

void free_global_history_item(struct global_history_item *i) {}
void delete_global_history_item(struct global_history_item *historyitem) {}
struct global_history_item *get_global_history_item(unsigned char *url, unsigned char *title, time_t time) {}
void add_global_history_item(unsigned char *ur, unsigned char *ti, time_t t) {}

#endif
