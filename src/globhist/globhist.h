/* $Id: globhist.h,v 1.19 2004/01/04 18:13:04 jonas Exp $ */

#ifndef EL__GLOBHIST_GLOBHIST_H
#define EL__GLOBHIST_GLOBHIST_H

struct listbox_item;
struct input_history;

#include "util/lists.h"
#include "util/ttime.h"

struct global_history_item {
	LIST_HEAD(struct global_history_item);

	struct listbox_item *box_item;

	unsigned char *title;
	unsigned char *url;

	ttime last_visit;
	int refcount; /* No direct access, use provided macros for that. */
};

extern struct input_history global_history;

extern unsigned char *gh_last_searched_title;
extern unsigned char *gh_last_searched_url;

extern struct module global_history_module;

void delete_global_history_item(struct global_history_item *);
struct global_history_item *get_global_history_item(unsigned char *);
void add_global_history_item(unsigned char *, unsigned char *, ttime);
int globhist_simple_search(unsigned char *, unsigned char *);

#endif
