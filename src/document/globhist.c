/* Global history */
/* $Id: globhist.c,v 1.5 2002/04/02 22:29:46 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>
#include <stdio.h>
#include <string.h>

#include <config/default.h>
#include <document/globhist.h>


#define GLOBHIST_MAX_ITEMS 4096

struct global_history_list global_history = {
	0,
	{ &global_history.items, &global_history.items }
};

/* GUI stuff. Declared here because finalize_global_history() frees it. */
unsigned char *gh_last_searched_title = NULL;
unsigned char *gh_last_searched_url = NULL;


#ifdef GLOBHIST


void
free_global_history_item(struct global_history_item *historyitem)
{
	mem_free(historyitem->title);
	mem_free(historyitem->url);
}

void
delete_global_history_item(struct global_history_item *historyitem)
{
	free_global_history_item(historyitem);
	del_from_list(historyitem);
	mem_free(historyitem);
	global_history.n--;
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


/* Add a new entry in history list, take care of duplicate, and respect history
 * size limit. add_to_history() doesn't handle last_visit, so I needed to make
 * this new function. */
void
add_global_history_item(unsigned char *url, unsigned char *title, time_t time)
{
	struct global_history_item *historyitem;

	if (!enable_global_history)
		return; 

	if (!title || !url)
		return;

	foreach (historyitem, global_history.items) {
		if (!strcmp(historyitem->url, url)) {
			delete_global_history_item(historyitem);
			break;
		}
	}

	historyitem = mem_alloc(sizeof(struct global_history_item));
	if (!historyitem)
		return;

	historyitem->last_visit = time;
	historyitem->title = stracpy(title);
	historyitem->url = stracpy(url);

	add_to_list(global_history.items, historyitem);
	global_history.n++;

	while (global_history.n > GLOBHIST_MAX_ITEMS) {
		historyitem = global_history.items.prev;

		if ((void *) historyitem == &global_history.items) {
			internal("global history is empty");
			global_history.n = 0;
			return;
		}

		delete_global_history_item(historyitem);
	}
}

void
read_global_history()
{
	unsigned char in_buffer[MAX_STR_LEN];
	unsigned char *file_name;
	unsigned char *title, *url, *last_visit;
	FILE *f;

	if (!enable_global_history)
		return; 

	file_name = stracpy(links_home);
	if (!file_name) return;
	add_to_strn(&file_name, "history");

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
	FILE *out;

	if (!enable_global_history)
		return; 

	file_name = stracpy(links_home);
	if (!file_name) return;
	add_to_strn(&file_name, "history");

	out = fopen(file_name, "w");
	mem_free(file_name);
	if (!out) return;

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

		fprintf(out, "%s\t%s\t%ld\n", historyitem->title,
			historyitem->url, historyitem->last_visit);
	}

	fclose(out);
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
