/* $Id: inphist.h,v 1.10 2003/10/29 14:09:50 pasky Exp $ */

#ifndef EL__BFU_INPHIST_H
#define EL__BFU_INPHIST_H

#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/lists.h"


struct input_history_item {
	LIST_HEAD(struct input_history_item);
	unsigned char d[1]; /* Must be last. */
};

struct input_history {
	struct list_head items;
	int size;
	unsigned int dirty:1;
	unsigned int nosave:1;
};

void add_to_input_history(struct input_history *, unsigned char *, int);

void do_tab_compl(struct terminal *, struct list_head *, struct window *);
void do_tab_compl_unambiguous(struct terminal *, struct list_head *, struct window *);

/* Load history file from elinks home. */
int load_input_history(struct input_history *history, unsigned char *filename);

/* Write history list to @filebane in elinks home. It returns a value different
 * from 0 in case of failure, 0 on success. */
int save_input_history(struct input_history *history, unsigned char *filename);

#endif
