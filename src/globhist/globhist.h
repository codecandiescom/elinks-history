/* $Id: globhist.h,v 1.7 2003/04/24 08:23:39 zas Exp $ */

#ifndef EL__GLOBHIST_GLOBHIST_H
#define EL__GLOBHIST_GLOBHIST_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "lowlevel/ttime.h"
#include "util/lists.h"

struct global_history_item {
	LIST_HEAD(struct global_history_item);

	ttime last_visit;
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
void finalize_global_history();

void delete_global_history_item(struct global_history_item *);
struct global_history_item *get_global_history_item(unsigned char *);
void add_global_history_item(unsigned char *, unsigned char *, ttime);
int globhist_simple_search(unsigned char *, unsigned char *);

#endif
