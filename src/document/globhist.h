/* $Id: globhist.h,v 1.3 2002/04/01 21:47:29 pasky Exp $ */

#ifndef EL__DOCUMENT_GLOBHIST_H
#define EL__DOCUMENT_GLOBHIST_H

#include <time.h>

#include "links.h" /* list_head */

struct global_history_item {
	struct global_history_item *next;
	struct global_history_item *prev;
	time_t last_visit;
	unsigned char *title;
	unsigned char *url;
};

struct global_history_list {
	int n;
	struct list_head items;
};

extern struct global_history_list global_history;

extern unsigned char *last_searched_globhist_title;
extern unsigned char *last_searched_globhist_url;

void read_global_history();
void write_global_history();
void finalize_global_history();

void free_global_history_item(struct global_history_item *);
void delete_global_history_item(struct global_history_item *historyitem);
struct global_history_item *get_global_history_item(unsigned char *url, unsigned char *title, time_t time);
void add_global_history_item(unsigned char *, unsigned char *, time_t);

#endif
