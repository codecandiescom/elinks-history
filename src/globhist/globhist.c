/* Global history */
/* $Id: globhist.c,v 1.76 2004/06/17 10:02:21 zas Exp $ */

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

#include "bfu/inphist.h" /* For struct input_history */
#include "bfu/listbox.h"
#include "config/options.h"
#include "globhist/dialogs.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "lowlevel/select.h"
#include "modules/module.h"
#include "util/file.h"
#include "util/hash.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"
#include "util/lists.h"
#include "util/object.h"
#include "util/ttime.h"

#define GLOBAL_HISTORY_FILENAME		"globhist"


INIT_INPUT_HISTORY(global_history);


/* GUI stuff. Declared here because done_global_history() frees it. */
unsigned char *gh_last_searched_title = NULL;
unsigned char *gh_last_searched_url = NULL;

/* Timer for periodically writing the history to disk. */
static int global_history_write_timer = -1;

enum global_history_options {
	GLOBHIST_TREE,

	GLOBHIST_ENABLE,
	GLOBHIST_MAX_ITEMS,
	GLOBHIST_DISPLAY_TYPE,
	GLOBHIST_WRITE_INTERVAL,

	GLOBHIST_OPTIONS,
};

static struct option_info global_history_options[] = {
	INIT_OPT_TREE("document.history", N_("Global history"),
		"global", 0,
		N_("Global history options.")),

	INIT_OPT_BOOL("document.history.global", N_("Enable"),
		"enable", 0, 1,
		N_("Enable global history (\"history of all pages visited\").")),

	INIT_OPT_INT("document.history.global", N_("Maximum number of entries"),
		"max_items", 0, 1, MAXINT, 1024,
		N_("Maximum number of entries in the global history.")),

	INIT_OPT_INT("document.history.global", N_("Display style"),
		"display_type", 0, 0, 1, 0,
		N_("What to display in global history dialog:\n"
		"0 is URLs\n"
		"1 is page titles")),

	INIT_OPT_INT("document.history.global", N_("Auto-save interval"),
		"write_interval", 0, 0, MAXINT, 300,
		N_("Interval at which to write global history to disk if it\n"
		"has changed (seconds; 0 to disable)")),

	NULL_OPTION_INFO,
};

#define get_opt_globhist(which)		global_history_options[(which)].option.value
#define get_globhist_enable()		get_opt_globhist(GLOBHIST_ENABLE).number
#define get_globhist_max_items()	get_opt_globhist(GLOBHIST_MAX_ITEMS).number
#define get_globhist_display_type()	get_opt_globhist(GLOBHIST_DISPLAY_TYPE).number
#define get_globhist_write_interval()	get_opt_globhist(GLOBHIST_WRITE_INTERVAL).number

static struct hash *globhist_cache = NULL;
static int globhist_cache_entries = 0;


void
delete_global_history_item(struct global_history_item *historyitem)
{
	del_from_history_list(&global_history, historyitem);

	if (globhist_cache) {
		struct hash_item *item;

		item = get_hash_item(globhist_cache, historyitem->url, strlen(historyitem->url));
		if (item) {
			del_hash_item(globhist_cache, item);
			globhist_cache_entries--;
		}
	}

	done_listbox_item(&globhist_browser, historyitem->box_item);
	mem_free(historyitem->title);
	mem_free(historyitem->url);
	mem_free(historyitem);
}

/* Search global history for item matching url. */
struct global_history_item *
get_global_history_item(unsigned char *url)
{
	struct hash_item *item;

	if (!url || !globhist_cache) return NULL;

	/* Search for cached entry. */

	item = get_hash_item(globhist_cache, url, strlen(url));

	return item ? (struct global_history_item *) item->value : NULL;
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

/* Add a new entry in history list, take care of duplicate, respect history
 * size limit, and update any open history dialogs. */
void
add_global_history_item(unsigned char *url, unsigned char *title, ttime vtime)
{
	struct global_history_item *history_item;
	unsigned char *text;
	int max_globhist_items;

	if (!url || !get_globhist_enable()) return;

	max_globhist_items = get_globhist_max_items();

	history_item = get_global_history_item(url);
	if (history_item) delete_global_history_item(history_item);

	while (global_history.size >= max_globhist_items) {
		history_item = global_history.entries.prev;

		if ((void *) history_item == &global_history.entries) {
			INTERNAL("global history is empty");
			global_history.size = 0;
			return;
		}

		delete_global_history_item(history_item);
	}

	history_item = mem_calloc(1, sizeof(struct global_history_item));
	if (!history_item)
		return;

	history_item->last_visit = vtime;
	history_item->title = stracpy(empty_string_or_(title));
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
	object_nolock(history_item, "globhist");

	add_to_history_list(&global_history, history_item);

	text = get_globhist_display_type()
		? history_item->title : history_item->url;
	if (!*text) text = history_item->url;

	history_item->box_item = add_listbox_leaf(&globhist_browser, NULL,
						  history_item);
	if (!history_item->box_item) return;

	/* Hash creation if needed. */
	if (!globhist_cache)
		globhist_cache = init_hash(8, &strhash);

	if (globhist_cache && globhist_cache_entries < max_globhist_items) {
		int urllen = strlen(history_item->url);

		/* Create a new entry. */
		if (add_hash_item(globhist_cache, history_item->url, urllen, history_item)) {
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
	mem_free_set(&gh_last_searched_title, stracpy(search_title));
	if (!gh_last_searched_title) return 0;

	/* Memorize last searched url */
	mem_free_set(&gh_last_searched_url, stracpy(search_url));
	if (!gh_last_searched_url) {
		mem_free(gh_last_searched_title);
		return 0;
	}

	if (!*search_title && !*search_url) {
		foreach (item, global_history.entries) {
			item->box_item->visible = 1;
		}
		return 1;
	}

	foreach (item, global_history.entries) {
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


static void
read_global_history(void)
{
	unsigned char in_buffer[MAX_STR_LEN];
	unsigned char *file_name = GLOBAL_HISTORY_FILENAME;
	unsigned char *title, *url, *last_visit;
	FILE *f;

	if (!get_globhist_enable()
	    || get_cmd_opt_int("anonymous"))
		return;

	if (elinks_home) {
		file_name = straconcat(elinks_home, file_name, NULL);
		if (!file_name) return;
	}
	f = fopen(file_name, "r");
	if (elinks_home) mem_free(file_name);
	if (!f) return;

	title = in_buffer;
	global_history.nosave = 1;

	while (fgets(in_buffer, MAX_STR_LEN, f)) {
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
	global_history.nosave = 0;
}

static void
write_global_history(void)
{
	struct global_history_item *historyitem;
	unsigned char *file_name;
	struct secure_save_info *ssi;

	if (!global_history.dirty || !elinks_home
	    || !get_globhist_enable())
		return;

	file_name = straconcat(elinks_home, GLOBAL_HISTORY_FILENAME, NULL);
	if (!file_name) return;

	ssi = secure_open(file_name, 0177); /* rw for user only */
	mem_free(file_name);
	if (!ssi) return;

	foreachback (historyitem, global_history.entries) {
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

	global_history.dirty = 0;
}

static void
free_global_history(void)
{
	if (globhist_cache) {
		free_hash(globhist_cache);
		globhist_cache = NULL;
		globhist_cache_entries = 0;
	}

	while (!list_empty(global_history.entries))
		delete_global_history_item(global_history.entries.next);
}

static void
global_history_write_timer_handler(void *xxx)
{
	int interval = get_globhist_write_interval();

	write_global_history();

	if (!interval) return;

	global_history_write_timer =
		install_timer(interval * 1000,
			      global_history_write_timer_handler,
			      NULL);
}

static int
global_history_write_timer_change_hook(struct session *ses,
				       struct option *current,
				       struct option *changed)
{
	if (global_history_write_timer >= 0) {
		kill_timer(global_history_write_timer);
		global_history_write_timer = -1;
	}

	if (elinks_home && !get_cmd_opt_int("anonymous"))
		global_history_write_timer_handler(NULL);

	return 0;
}

static void
init_global_history(struct module *module)
{
	struct change_hook_info global_history_change_hooks[] = {
		{ "document.history.global.write_interval",
		  global_history_write_timer_change_hook },
		{ NULL,	NULL },
	};

	register_change_hooks(global_history_change_hooks);
	read_global_history();

	if (elinks_home && !get_cmd_opt_int("anonymous"))
		global_history_write_timer_handler(NULL);
}

static void
done_global_history(struct module *module)
{
	if (global_history_write_timer >= 0)
		kill_timer(global_history_write_timer);
	if (elinks_home && !get_cmd_opt_int("anonymous"))
		write_global_history();
	free_global_history();
	mem_free_if(gh_last_searched_title);
	mem_free_if(gh_last_searched_url);
}

struct module global_history_module = struct_module(
	/* name: */		N_("Global History"),
	/* options: */		global_history_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_global_history,
	/* done: */		done_global_history
);
