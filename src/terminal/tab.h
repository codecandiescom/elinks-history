/* $Id: tab.h,v 1.3 2003/05/05 14:06:47 zas Exp $ */

#ifndef EL__TERMINAL_TAB_H
#define EL__TERMINAL_TAB_H

#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/lists.h"

#define get_current_tab(term) get_tab_by_number((term), (term)->current_tab)
#define IF_ACTIVE(win,term) if (!(win)->type || (win) == get_current_tab((term)))

struct window *init_tab(struct terminal *);
int number_of_tabs(struct terminal *);
int get_tab_number(struct window *);
struct window *get_tab_by_number(struct terminal *, int);
void switch_to_tab(struct terminal *, int);
void close_tab(struct terminal *);

#endif
