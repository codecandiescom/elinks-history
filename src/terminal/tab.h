/* $Id: tab.h,v 1.2 2003/05/04 20:35:13 pasky Exp $ */

#ifndef EL__TERMINAL_TAB_H
#define EL__TERMINAL_TAB_H

#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/lists.h"

#define IF_ACTIVE(win,term) if(!(win)->type || (win)==get_tab_by_number((term),(term)->current_tab))

struct window *init_tab(struct terminal *);
int number_of_tabs(struct terminal *term);
int get_tab_number(struct window *window);
struct window *get_tab_by_number(struct terminal *term, int num);
void switch_to_tab(struct terminal *term, int num);
void close_tab(struct terminal *term);
#define get_current_tab(term) get_tab_by_number((term), (term)->current_tab)

#endif
