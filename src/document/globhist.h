/* $Id: globhist.h,v 1.2 2002/04/01 20:48:36 pasky Exp $ */

#ifndef EL__DOCUMENT_GLOBHIST_H
#define EL__DOCUMENT_GLOBHIST_H

#include <time.h>


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

void read_global_history();
void write_global_history();
void finalize_global_history();

void free_global_history_item(struct global_history_item *);
void delete_global_history_item(struct global_history_item *historyitem);
struct global_history_item *get_global_history_item(unsigned char *url, unsigned char *title, time_t time);
void add_global_history_item(unsigned char *, unsigned char *, time_t);

#endif
