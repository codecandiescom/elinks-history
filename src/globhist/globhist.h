/* $Id: globhist.h,v 1.3 2002/08/31 00:27:54 pasky Exp $ */

#ifndef EL__GLOBHIST_GLOBHIST_H
#define EL__GLOBHIST_GLOBHIST_H

#include <time.h>

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "util/lists.h"

struct global_history_item {
	struct global_history_item *next;
	struct global_history_item *prev;

	time_t last_visit;
	unsigned char *title;
	unsigned char *url;
	int refcount;

	struct listbox_item *box_item;
};

struct global_history_list {
	int n;
	struct list_head items;
};

extern struct global_history_list global_history;
extern struct list_head gh_box_items;
extern struct list_head gh_boxes;

extern unsigned char *gh_last_searched_title;
extern unsigned char *gh_last_searched_url;

void read_global_history();
void write_global_history();
void finalize_global_history();

void free_global_history_item(struct global_history_item *);
void delete_global_history_item(struct global_history_item *historyitem);
struct global_history_item *get_global_history_item(unsigned char *url, unsigned char *title, time_t time);
void add_global_history_item(unsigned char *, unsigned char *, time_t);
int globhist_simple_search(unsigned char *, unsigned char *);

#endif
