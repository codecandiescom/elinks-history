/* $Id: inphist.h,v 1.4 2003/05/04 19:54:32 pasky Exp $ */

#ifndef EL__BFU_INPHIST_H
#define EL__BFU_INPHIST_H

#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/lists.h"


struct input_history_item {
	LIST_HEAD(struct input_history_item);
	unsigned char d[1];
};

struct input_history {
	int n;
	struct list_head items;
};

void add_to_input_history(struct input_history *, unsigned char *, int);

void do_tab_compl(struct terminal *, struct list_head *, struct window *);
void do_tab_compl_unambiguous(struct terminal *, struct list_head *, struct window *);

#endif
