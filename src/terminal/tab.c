/* Tab-style (those containing real documents) windows infrastructure. */
/* $Id: tab.c,v 1.2 2003/05/04 20:10:56 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/lists.h"


struct window *
init_tab(struct terminal *term)
{
	struct window *win = mem_calloc(1, sizeof(struct window));
	struct window *current_tab = get_root_window(term);

	if (!win) return NULL;

	win->handler = current_tab ? current_tab->handler : NULL;
	win->term = term;
	win->type = WT_ROOT;

	add_to_list(term->windows, win);
	term->current_tab = get_tab_number(win);

	return win;
}

/* Number of tabs - just number of root windows in term->windows */
inline int
number_of_tabs(struct terminal *term)
{
	int result = 0;
	struct window *win;

	foreach (win, term->windows) {
		result += (win->type == WT_ROOT);
	}

	return result;
}

/* Number of tab */
int
get_tab_number(struct window *window)
{
	struct terminal *term = window->term;
	struct window *win;
        int current = 0;
	int num = 0;

	foreachback (win, term->windows) {
		if (win == window) {
			num = current;
			break;
		}
		current += (win->type == WT_ROOT);
	}

	return num;
}

/* Get root window of a given tab */
struct window *
get_tab_by_number(struct terminal *term, int num)
{
	struct window *win = NULL;

	foreachback (win, term->windows) {
		if (win->type == WT_ROOT && !num)
			break;
		num -= win->type;
	}

	return win;
}

void
switch_to_tab(struct terminal *term, int num)
{
	int num_tabs = number_of_tabs(term);

	if (num >= num_tabs) {
		if (get_opt_bool("ui.tabs.wraparound"))
			num = 0;
		else
			num = num_tabs - 1;
	}

	if (num < 0) {
		if (get_opt_bool("ui.tabs.wraparound"))
			num = num_tabs - 1;
		else
			num = 0;
	}

	if (num != term->current_tab) {
		term->current_tab = num;

		redraw_terminal_cls(term);
	}
}

void
close_tab(struct terminal *term)
{
	if (number_of_tabs(term) < 2)
		return;

	delete_window(get_root_window(term));

	switch_to_tab(term, term->current_tab);
}
