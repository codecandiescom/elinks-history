/* $Id: tab.h,v 1.22 2003/12/27 22:18:01 jonas Exp $ */

#ifndef EL__TERMINAL_TAB_H
#define EL__TERMINAL_TAB_H

#include "terminal/terminal.h"
#include "terminal/window.h"

struct session;

struct window *init_tab(struct terminal *, int in_background);
int number_of_tabs(struct terminal *);
int get_tab_number(struct window *);
int get_tab_number_by_xpos(struct terminal *term, int xpos);
struct window *get_tab_by_number(struct terminal *, int);
void switch_to_tab(struct terminal *, int, int);
void switch_to_next_tab(struct terminal *term);
void switch_to_prev_tab(struct terminal *term);
void close_tab(struct terminal *, struct session *);
void close_all_tabs_but_current(struct terminal *term, void *d, struct session *ses);

#define get_current_tab(term) get_tab_by_number((term), (term)->current_tab)
#define inactive_tab(win) ((win)->type != WT_NORMAL && (win) != get_current_tab((win->term)))

void open_url_in_new_tab(struct session *ses, unsigned char *url, int in_background);
void open_in_new_tab(struct terminal *term, int link, struct session *ses);
void open_in_new_tab_in_background(struct terminal *term, int link, struct session *ses);

#define foreach_tab(tab, terminal) \
	foreach (tab, terminal) if (tab->type == WT_TAB)

#define foreachback_tab(tab, terminal) \
	foreachback (tab, terminal) if (tab->type == WT_TAB)

#endif
