/* Tab-style (those containing real documents) windows infrastructure. */
/* $Id: tab.c,v 1.21 2003/10/18 21:43:26 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "terminal/screen.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/lists.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"


struct window *
init_tab(struct terminal *term, int in_background)
{
	struct window *win = mem_calloc(1, sizeof(struct window));
	struct window *current_tab = get_current_tab(term);

	if (!win) return NULL;

	win->handler = current_tab ? current_tab->handler : NULL;
	win->term = term;
	win->type = WT_TAB;

	add_to_list(term->windows, win);
	if (!in_background && current_tab)
		term->current_tab = get_tab_number(win);

	return win;
}

/* Number of tabs at the terminal (in term->windows) */
inline int
number_of_tabs(struct terminal *term)
{
	int result = 0;
	struct window *win;

	foreach (win, term->windows) {
		result += (win->type == WT_TAB);
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
		current += (win->type == WT_TAB);
	}

	return num;
}

/* Get tab of an according index */
struct window *
get_tab_by_number(struct terminal *term, int num)
{
	struct window *win = NULL;

	foreachback (win, term->windows) {
		if (win->type == WT_TAB && !num)
			break;
		num -= win->type;
	}

	return win;
}

/* Returns number of the tab at @xpos, or -1 if none. */
int
get_tab_number_by_xpos(struct terminal *term, int xpos)
{
	int num = 0;
	struct window *win = NULL;

	foreachback (win, term->windows) {
		if (win->type != WT_TAB) continue;
		if (xpos >= win->xpos && xpos <= win->xpos + win->width)
			return num;
		num++;
	}

	return -1;
}

/* if nbtabs > 0, then it is taken as the result of a recent
 * call to number_of_tabs() so it just uses this value. */
void
switch_to_tab(struct terminal *term, int num, int nbtabs)
{
	int num_tabs = nbtabs;

	if (nbtabs < 0) num_tabs = number_of_tabs(term);
	if (num_tabs > 1) {
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
	} else num = 0;

	if (num != term->current_tab) {
		term->current_tab = num;
		set_screen_dirty(term->screen, 0, term->y);
		redraw_terminal(term);
	}
}

void
switch_to_next_tab(struct terminal *term)
{
	int num_tabs = number_of_tabs(term);

	if (num_tabs < 2)
		return;

	switch_to_tab(term, term->current_tab + 1, num_tabs);
}

void
switch_to_prev_tab(struct terminal *term)
{
	int num_tabs = number_of_tabs(term);

	if (num_tabs < 2)
		return;

	switch_to_tab(term, term->current_tab - 1, num_tabs);
}

void
close_tab(struct terminal *term)
{
	int num_tabs = number_of_tabs(term);

	if (num_tabs < 2)
		return;

	delete_window(get_current_tab(term));
	switch_to_tab(term, term->current_tab - 1, num_tabs - 1);
}


static void
do_open_in_new_tab(struct terminal *term, struct session *ses,
		   unsigned char *url, int in_background)
{
	struct window *tab;
	struct initial_session_info *info;
	struct term_event ev = INIT_TERM_EVENT(EV_INIT, 0, 0, 0);

	tab = init_tab(term, in_background);
	if (!tab) return;

	info = mem_calloc(1, sizeof(struct initial_session_info));
	if (!info) {
		mem_free(tab);
		return;
	}

	info->base_session = ses->id;
	info->url = url;

	ev.b = (long) info;
	tab->handler(tab, &ev, 0);
}

void
open_in_new_tab(struct terminal *term, void *xxx, struct session *ses)
{
	do_open_in_new_tab(term, ses, NULL, 0);
}

void
open_in_new_tab_in_background(struct terminal *term, void *xxx,
			      struct session *ses)
{
	struct document_view *doc_view;

	assert(ses);

	doc_view = current_frame(ses);
	if (doc_view) assert(doc_view->vs && doc_view->document);
	if_assert_failed return;

	do_open_in_new_tab(term, ses,
			   doc_view && doc_view->vs->current_link != -1
			   	? get_link_url(doc_view->document->links[doc_view->vs->current_link])
				: NULL,
			   1);
}
