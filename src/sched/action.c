/* Sessions action management */
/* $Id: action.c,v 1.18 2004/01/08 00:29:58 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"
#include "main.h"

#include "bookmarks/dialogs.h"
#include "cache/dialogs.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "cookies/cookies.h"
#include "cookies/dialogs.h"
#include "dialogs/document.h"
#include "dialogs/download.h"
#include "dialogs/menu.h"
#include "dialogs/options.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/view.h"
#include "formhist/dialogs.h"
#include "globhist/dialogs.h"
#include "protocol/auth/auth.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "sched/action.h"
#include "sched/connection.h"
#include "sched/event.h"
#include "sched/session.h"
#include "sched/task.h"
#include "viewer/text/search.h"
#include "viewer/text/view.h"


static void
toggle_document_option(struct session *ses, unsigned char *option_name)
{
	struct option *option;
	long number;

	assert(ses && ses->doc_view && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!ses->doc_view->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	option = get_opt_rec(config_options, option_name);
	number = option->value.number + 1;

	assert(option->type == OPT_BOOL || option->type == OPT_INT);
	assert(option->max);

	/* TODO: toggle per document. --Zas */
	option->value.number = (number <= option->max) ? number : option->min;

	draw_formatted(ses, 1);
}

static void
do_frame_action(struct session *ses,
	       void (*func)(struct session *, struct document_view *, int))
{
	struct document_view *doc_view;

	assert(ses && func);
	if_assert_failed return;

	if (!have_location(ses)) return;

	doc_view = current_frame(ses);

	assertm(doc_view, "document not formatted");
	if_assert_failed return;

	assertm(doc_view->vs, "document view has no state");
	if_assert_failed return;

	func(ses, doc_view, 0);
}


/* This could gradually become some mulitplexor / switch noodle containing
 * most if not all default handling of actions (for the main mapping) that
 * frame_ev() and/or send_event() could use as a backend. */
enum keyact
do_action(struct session *ses, enum keyact action, int verbose)
{
	struct terminal *term = ses->tab->term;
	struct document_view *doc_view = doc_view = current_frame(ses);

	switch (action) {
		/* Please keep in alphabetical order for now. Later we can sort
		 * by most used or something. */
		case ACT_ABORT_CONNECTION:
			abort_loading(ses, 1);
			print_screen_status(ses);
			break;

		case ACT_ADD_BOOKMARK:
#ifdef CONFIG_BOOKMARKS
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				launch_bm_add_doc_dialog(ses->tab->term, NULL, ses);
#endif
			break;
		case ACT_ADD_BOOKMARK_LINK:
#ifdef CONFIG_BOOKMARKS
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				launch_bm_add_link_dialog(ses->tab->term, NULL, ses);
#endif
			break;
		case ACT_ADD_BOOKMARK_TABS:
#ifdef CONFIG_BOOKMARKS
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				bookmark_terminal_tabs_dialog(term);
#endif
			break;

		case ACT_BACK:
			go_back(ses);
			break;

		case ACT_BOOKMARK_MANAGER:
#ifdef CONFIG_BOOKMARKS
			bookmark_manager(ses);
#endif
			break;

		case ACT_CACHE_MANAGER:
			cache_manager(ses);
			break;

		case ACT_CACHE_MINIMIZE:
			shrink_memory(1);
			break;

		case ACT_COOKIES_LOAD:
#ifdef CONFIG_COOKIES
			if (!get_opt_int_tree(cmdline_options, "anonymous")
			    && get_opt_int("cookies.save"))
				load_cookies();
#endif
			break;

		case ACT_COOKIE_MANAGER:
#ifdef CONFIG_COOKIES
			cookie_manager(ses);
#endif
			break;

		case ACT_DOCUMENT_INFO:
			state_msg(ses);
			break;

		case ACT_DOWNLOAD_MANAGER:
			download_manager(ses);
			break;

		case ACT_FILE_MENU:
			activate_bfu_technology(ses, 0);
			break;

		case ACT_FIND_NEXT:
			do_frame_action(ses, find_next);
			break;

		case ACT_FIND_NEXT_BACK:
			do_frame_action(ses, find_next_back);
			break;

		case ACT_FORGET_CREDENTIALS:
			free_auth();
			shrink_memory(1); /* flush caches */
			break;

		case ACT_FORMHIST_MANAGER:
#ifdef CONFIG_FORMHIST
			formhist_manager(ses);
#endif
			break;

		case ACT_GOTO_URL:
			dialog_goto_url(ses, "");
			break;

		case ACT_GOTO_URL_HOME:
		{
			unsigned char *url = getenv("WWW_HOME");

			if (!url || !*url) url = WWW_HOME_URL;
			goto_url_with_hook(ses, url);
			break;
		}
		case ACT_HEADER_INFO:
			head_msg(ses);
			break;

		case ACT_HISTORY_MANAGER:
#ifdef CONFIG_GLOBHIST
			history_manager(ses);
#endif
			break;

		case ACT_KEYBINDING_MANAGER:
			keybinding_manager(ses);
			break;

		case ACT_KILL_BACKGROUNDED_CONNECTIONS:
			abort_background_connections();
			break;

		case ACT_MENU:
			activate_bfu_technology(ses, -1);
			break;

		case ACT_NEXT_FRAME:
			next_frame(ses, 1);
			draw_formatted(ses, 0);
			break;

		case ACT_OPEN_LINK_IN_NEW_TAB:
			open_in_new_tab(term, 1, ses);
			break;

		case ACT_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND:
			open_in_new_tab_in_background(term, 1, ses);
			break;

		case ACT_OPEN_LINK_IN_NEW_WINDOW:
			/* FIXME: Use do_frame_action(). --jonas */
			if (!doc_view || doc_view->vs->current_link == -1) break;
			open_in_new_window(term, send_open_in_new_window, ses);
			break;

		case ACT_OPEN_NEW_TAB:
			open_in_new_tab(term, 0, ses);
			break;

		case ACT_OPEN_NEW_TAB_IN_BACKGROUND:
			open_in_new_tab_in_background(term, 0, ses);
			break;

		case ACT_OPEN_NEW_WINDOW:
			open_in_new_window(term, send_open_new_window, ses);
			break;

		case ACT_OPEN_OS_SHELL:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				exec_shell(term);
			break;

		case ACT_OPTIONS_MANAGER:
			options_manager(ses);
			break;

		case ACT_PREVIOUS_FRAME:
			next_frame(ses, -1);
			draw_formatted(ses, 0);
			break;

		case ACT_REALLY_QUIT:
			exit_prog(term, (void *)1, ses);
			break;

		case ACT_REDRAW:
			redraw_terminal_cls(term);
			break;

		case ACT_RELOAD:
			reload(ses, CACHE_MODE_INCREMENT);
			break;

		case ACT_SAVE_AS:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				save_as(term, NULL, ses);
			break;

		case ACT_SAVE_FORMATTED:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				do_frame_action(ses, save_formatted_dlg);
			break;

		case ACT_SAVE_URL_AS:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				save_url_as(ses);
			break;

		case ACT_SAVE_OPTIONS:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				write_config(term);
			break;

		case ACT_SEARCH:
			do_frame_action(ses, search_dlg);
			break;

		case ACT_SEARCH_BACK:
			do_frame_action(ses, search_back_dlg);
			break;

		case ACT_SEARCH_TYPEAHEAD:
			do_frame_action(ses, search_typeahead);
			break;

		case ACT_SHOW_TERM_OPTIONS:
			terminal_options(term, NULL, ses);
			break;

		case ACT_TAB_NEXT:
			switch_to_next_tab(term);
			break;

		case ACT_TAB_MENU:
			assert(ses->tab == get_current_tab(term));

			if (ses->status.show_tabs_bar)
				set_window_ptr(ses->tab, ses->tab->xpos, term->height - 2);
			else
				set_window_ptr(ses->tab, 0, 0);

			tab_menu(term, ses->tab, ses);
			break;

		case ACT_TAB_PREV:
			switch_to_prev_tab(term);
			break;

		case ACT_TAB_CLOSE:
			close_tab(term, ses);
			break;

		case ACT_TAB_CLOSE_ALL_BUT_CURRENT:
			close_all_tabs_but_current(ses);
			break;

		case ACT_TOGGLE_DISPLAY_IMAGES:
			toggle_document_option(ses, "document.browse.images.show_as_links");
			break;

		case ACT_TOGGLE_DISPLAY_TABLES:
			toggle_document_option(ses, "document.html.display_tables");
			break;

		case ACT_TOGGLE_DOCUMENT_COLORS:
			toggle_document_option(ses, "document.colors.use_document_colors");
			break;

		case ACT_TOGGLE_HTML_PLAIN:
			toggle_plain_html(ses, ses->doc_view, 0);
			break;

		case ACT_TOGGLE_NUMBERED_LINKS:
			toggle_document_option(ses, "document.browse.links.numbering");
			break;

		case ACT_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES:
			toggle_document_option(ses, "document.plain.compress_empty_lines");
			break;

		case ACT_UNBACK:
			go_unback(ses);
			break;

		case ACT_ZOOM_FRAME:
			do_frame_action(ses, set_frame);
			break;

		case ACT_AUTO_COMPLETE:
		case ACT_AUTO_COMPLETE_UNAMBIGUOUS:
		case ACT_BACKSPACE:
		case ACT_BEGINNING_OF_BUFFER:
		case ACT_CANCEL:
		case ACT_COPY_CLIPBOARD:
		case ACT_CUT_CLIPBOARD:
		case ACT_DELETE:
		case ACT_DOWN:
		case ACT_DOWNLOAD:
		case ACT_DOWNLOAD_IMAGE:
		case ACT_EDIT:
		case ACT_END:
		case ACT_END_OF_BUFFER:
		case ACT_ENTER:
		case ACT_ENTER_RELOAD:
		case ACT_EXPAND:
		case ACT_GOTO_URL_CURRENT:
		case ACT_GOTO_URL_CURRENT_LINK:
		case ACT_HOME:
		case ACT_KILL_TO_BOL:
		case ACT_KILL_TO_EOL:
		case ACT_LEFT:
		case ACT_LINK_MENU:
		case ACT_JUMP_TO_LINK:
		case ACT_LUA_CONSOLE:
		case ACT_MARK_SET:
		case ACT_MARK_GOTO:
		case ACT_MARK_ITEM:
		case ACT_NEXT_ITEM:
		case ACT_PAGE_DOWN:
		case ACT_PAGE_UP:
		case ACT_PASTE_CLIPBOARD:
		case ACT_QUIT:
		case ACT_RESUME_DOWNLOAD:
		case ACT_RIGHT:
		case ACT_SCRIPTING_FUNCTION:
		case ACT_SCROLL_DOWN:
		case ACT_SCROLL_LEFT:
		case ACT_SCROLL_RIGHT:
		case ACT_SCROLL_UP:
		case ACT_SELECT:
		case ACT_UNEXPAND:
		case ACT_UP:
		case ACT_VIEW_IMAGE:
		default:
			if (verbose) {
				INTERNAL("No action handling defined for '%s'.",
					 write_action(action));
			}

			return ACT_NONE;
	}

	return action;
}
