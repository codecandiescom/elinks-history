/* Tab-style (those containing real documents) windows infrastructure. */
/* $Id: tab.c,v 1.49 2004/02/26 00:50:56 miciah Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/msgbox.h"
#include "config/options.h"
#include "dialogs/menu.h"
#include "document/document.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "sched/session.h"
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

	foreach_tab (win, term->windows)
		result++;

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

	foreachback_tab (win, term->windows) {
		if (win == window) {
			num = current;
			break;
		}
		current++;
	}

	return num;
}

/* Get tab of an according index */
struct window *
get_tab_by_number(struct terminal *term, int num)
{
	struct window *win = NULL;

	foreachback_tab (win, term->windows) {
		if (!num) break;
		num--;
	}

	return win;
}

/* Returns number of the tab at @xpos, or -1 if none. */
int
get_tab_number_by_xpos(struct terminal *term, int xpos)
{
	int num = 0;
	struct window *win = NULL;

	foreachback_tab (win, term->windows) {
		if (xpos >= win->xpos
		    && xpos < win->xpos + win->width - 1)
			return num;
		num++;
	}

	return -1;
}

/* if @tabs > 0, then it is taken as the result of a recent
 * call to number_of_tabs() so it just uses this value. */
void
switch_to_tab(struct terminal *term, int tab, int tabs)
{
	if (tabs < 0) tabs = number_of_tabs(term);

	if (tabs > 1) {
		if (tab >= tabs) {
			if (get_opt_bool("ui.tabs.wraparound"))
				tab = 0;
			else
				tab = tabs - 1;
		}

		if (tab < 0) {
			if (get_opt_bool("ui.tabs.wraparound"))
				tab = tabs - 1;
			else
				tab = 0;
		}
	} else tab = 0;

	if (tab != term->current_tab) {
		term->current_tab = tab;
		set_screen_dirty(term->screen, 0, term->height);
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

static void
really_close_tab(struct session *ses)
{
	struct terminal *term = ses->tab->term;
	int num_tabs = number_of_tabs(term);
	struct window *current_tab = get_current_tab(term);

	switch_to_tab(term, term->current_tab - 1, num_tabs - 1);
	delete_window(current_tab);
}

void
close_tab(struct terminal *term, struct session *ses)
{
	int num_tabs = number_of_tabs(term);

	if (num_tabs < 2) {
		query_exit(ses);
		return;
	}

	if (!get_opt_bool("ui.tabs.confirm_close")) {
		really_close_tab(ses);
		return;
	}

	msg_box(term, NULL, 0,
		N_("Close tab"), AL_CENTER,
		N_("Do you really want to close the current tab?"),
		ses, 2,
		N_("Yes"), (void (*)(void *)) really_close_tab, B_ENTER,
		N_("No"), NULL, B_ESC);
}

static void
really_close_tabs(struct session *ses)
{
	struct terminal *term = ses->tab->term;
	struct window *current = get_current_tab(term);
	struct window *tab;

	foreach_tab (tab, term->windows) {
		if (tab == current) continue;
		tab = tab->prev;
		delete_window(tab->next);
	}

	term->current_tab = 0;
	redraw_terminal(term);
}

void
close_all_tabs_but_current(struct session *ses)
{
	assert(ses);
	if_assert_failed return;

	if (!get_opt_bool("ui.tabs.confirm_close")) {
		really_close_tabs(ses);
		return;
	}

	msg_box(ses->tab->term, NULL, 0,
		N_("Close tab"), AL_CENTER,
		N_("Do you really want to close all except the current tab?"),
		ses, 2,
		N_("Yes"), (void (*)(void *)) really_close_tabs, B_ENTER,
		N_("No"), NULL, B_ESC);
}


void
open_url_in_new_tab(struct session *ses, unsigned char *url, int in_background)
{
	struct window *tab;
	struct initial_session_info *info;
	struct term_event ev = INIT_TERM_EVENT(EV_INIT, 0, 0, 0);

	assert(ses);

	tab = init_tab(ses->tab->term, in_background);
	if (!tab) return;

	info = mem_calloc(1, sizeof(struct initial_session_info));
	if (!info) {
		mem_free(tab);
		return;
	}

	info->base_session = ses->id;
	init_list(info->url_list);
	if (url) add_to_string_list(&info->url_list, url, -1);

	ev.b = (long) info;
	tab->handler(tab, &ev, 0);
}

void
open_current_link_in_new_tab(struct session *ses, int in_background)
{
	struct document_view *doc_view = current_frame(ses);
	unsigned char *url = NULL;

	if (doc_view) assert(doc_view->vs && doc_view->document);
	if_assert_failed return;

	if (doc_view && doc_view->vs->current_link != -1)
		url = get_link_url(ses, doc_view,
			&doc_view->document->links[doc_view->vs->current_link]);

	open_url_in_new_tab(ses, url, in_background);
	if (url) mem_free(url);
}
