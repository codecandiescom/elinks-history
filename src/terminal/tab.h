/* $Id: tab.h,v 1.6 2003/05/08 22:24:45 pasky Exp $ */

#ifndef EL__TERMINAL_TAB_H
#define EL__TERMINAL_TAB_H

#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/lists.h"

struct window *init_tab(struct terminal *);
int number_of_tabs(struct terminal *);
int get_tab_number(struct window *);
struct window *get_tab_by_number(struct terminal *, int);
void switch_to_tab(struct terminal *, int, int);
void close_tab(struct terminal *);

#define get_current_tab(term) get_tab_by_number((term), (term)->current_tab)
#define IF_ACTIVE(win, term) if ((win)->type == WT_NORMAL || (win) == get_current_tab((term)))

#endif
