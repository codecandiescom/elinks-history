/* Sessions action management */
/* $Id: action.c,v 1.65 2004/05/25 07:14:50 jonas Exp $ */

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
#include "dialogs/exmode.h"
#include "dialogs/info.h"
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
#include "viewer/text/form.h"
#include "viewer/text/link.h"
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

typedef void (*frame_action)(struct session *, struct document_view *, int);

static void
do_frame_action(struct session *ses, frame_action action, int magic)
{
	struct document_view *doc_view;

	assert(ses && action);
	if_assert_failed return;

	if (!have_location(ses)) return;

	doc_view = current_frame(ses);

	assertm(doc_view, "document not formatted");
	if_assert_failed return;

	assertm(doc_view->vs, "document view has no state");
	if_assert_failed return;

	action(ses, doc_view, magic);

	/* This is hopefully only some temporary setup. --jonas */
	if (action == find_next) {
		draw_doc(ses->tab->term, doc_view, 1);
		print_screen_status(ses);
		redraw_from_window(ses->tab);
	}
}

static void
goto_url_action(struct session *ses,
		unsigned char *(*get_url)(struct session *, unsigned char *, size_t))
{
	unsigned char url[MAX_STR_LEN];

	if (!get_url || !get_url(ses, url, sizeof(url)))
		url[0] = 0;

	dialog_goto_url(ses, url);
}

/* This could gradually become some multiplexor / switch noodle containing
 * most if not all default handling of actions (for the main mapping) that
 * frame_ev() and/or send_event() could use as a backend. */
enum main_action
do_action(struct session *ses, enum main_action action, int verbose)
{
	struct terminal *term = ses->tab->term;
	struct document_view *doc_view = current_frame(ses);

	switch (action) {
		/* Please keep in alphabetical order for now. Later we can sort
		 * by most used or something. */
		case ACT_MAIN_ABORT_CONNECTION:
			abort_loading(ses, 1);
			print_screen_status(ses);
			break;

		case ACT_MAIN_ADD_BOOKMARK:
#ifdef CONFIG_BOOKMARKS
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				launch_bm_add_doc_dialog(term, NULL, ses);
#endif
			break;
		case ACT_MAIN_ADD_BOOKMARK_LINK:
#ifdef CONFIG_BOOKMARKS
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				launch_bm_add_link_dialog(term, NULL, ses);
#endif
			break;
		case ACT_MAIN_ADD_BOOKMARK_TABS:
#ifdef CONFIG_BOOKMARKS
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				bookmark_terminal_tabs_dialog(term);
#endif
			break;

		case ACT_MAIN_BACK:
			go_back(ses);
			break;

		case ACT_MAIN_BOOKMARK_MANAGER:
#ifdef CONFIG_BOOKMARKS
			bookmark_manager(ses);
#endif
			break;

		case ACT_MAIN_CACHE_MANAGER:
			cache_manager(ses);
			break;

		case ACT_MAIN_CACHE_MINIMIZE:
			shrink_memory(1);
			break;

		case ACT_MAIN_COOKIES_LOAD:
#ifdef CONFIG_COOKIES
			if (!get_opt_int_tree(cmdline_options, "anonymous")
			    && get_opt_int("cookies.save"))
				load_cookies();
#endif
			break;

		case ACT_MAIN_COOKIE_MANAGER:
#ifdef CONFIG_COOKIES
			cookie_manager(ses);
#endif
			break;

		case ACT_MAIN_DOCUMENT_INFO:
			state_msg(ses);
			break;

		case ACT_MAIN_DOWNLOAD:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				do_frame_action(ses, download_link, action);
			break;

		case ACT_MAIN_DOWNLOAD_IMAGE:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				do_frame_action(ses, download_link, action);
			break;

		case ACT_MAIN_DOWNLOAD_MANAGER:
			download_manager(ses);
			break;

		case ACT_MAIN_ENTER:
			do_frame_action(ses, (frame_action) enter, 0);
			break;

		case ACT_MAIN_ENTER_RELOAD:
			do_frame_action(ses, (frame_action) enter, 1);
			break;

		case ACT_MAIN_EXMODE:
#ifdef CONFIG_EXMODE
			exmode_start(ses);
#endif
			break;

		case ACT_MAIN_FILE_MENU:
			activate_bfu_technology(ses, 0);
			break;

		case ACT_MAIN_FIND_NEXT:
			do_frame_action(ses, find_next, 1);
			break;

		case ACT_MAIN_FIND_NEXT_BACK:
			do_frame_action(ses, find_next, -1);
			break;

		case ACT_MAIN_FORGET_CREDENTIALS:
			free_auth();
			shrink_memory(1); /* flush caches */
			break;

		case ACT_MAIN_FORMHIST_MANAGER:
#ifdef CONFIG_FORMHIST
			formhist_manager(ses);
#endif
			break;

		case ACT_MAIN_GOTO_URL:
			goto_url_action(ses, NULL);
			break;

		case ACT_MAIN_GOTO_URL_CURRENT:
			goto_url_action(ses, get_current_url);
			break;

		case ACT_MAIN_GOTO_URL_CURRENT_LINK:
			goto_url_action(ses, get_current_link_url);
			break;

		case ACT_MAIN_GOTO_URL_HOME:
			goto_url_home(ses);
			break;

		case ACT_MAIN_HEADER_INFO:
			head_msg(ses);
			break;

		case ACT_MAIN_HISTORY_MANAGER:
#ifdef CONFIG_GLOBHIST
			history_manager(ses);
#endif
			break;

		case ACT_MAIN_KEYBINDING_MANAGER:
			keybinding_manager(ses);
			break;

		case ACT_MAIN_KILL_BACKGROUNDED_CONNECTIONS:
			abort_background_connections();
			break;

		case ACT_MAIN_LINK_MENU:
			link_menu(term, NULL, ses);
			break;

		case ACT_MAIN_LUA_CONSOLE:
#ifdef CONFIG_LUA
			trigger_event_name("dialog-lua-console", ses);
#endif
			break;

		case ACT_MAIN_MENU:
			activate_bfu_technology(ses, -1);
			break;

		case ACT_MAIN_NEXT_FRAME:
			next_frame(ses, 1);
			draw_formatted(ses, 0);
			break;

		case ACT_MAIN_OPEN_LINK_IN_NEW_TAB:
			open_current_link_in_new_tab(ses, 0);
			break;

		case ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND:
			open_current_link_in_new_tab(ses, 1);
			break;

		case ACT_MAIN_OPEN_LINK_IN_NEW_WINDOW:
			/* FIXME: Use do_frame_action(). --jonas */
			if (!doc_view || doc_view->vs->current_link == -1) break;
			open_in_new_window(term, send_open_in_new_window, ses);
			break;

		case ACT_MAIN_OPEN_NEW_TAB:
			open_url_in_new_tab(ses, NULL, 0);
			break;

		case ACT_MAIN_OPEN_NEW_TAB_IN_BACKGROUND:
			open_url_in_new_tab(ses, NULL, 1);
			break;

		case ACT_MAIN_OPEN_NEW_WINDOW:
			open_in_new_window(term, send_open_new_window, ses);
			break;

		case ACT_MAIN_OPEN_OS_SHELL:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				exec_shell(term);
			break;

		case ACT_MAIN_OPTIONS_MANAGER:
			options_manager(ses);
			break;

		case ACT_MAIN_PREVIOUS_FRAME:
			next_frame(ses, -1);
			draw_formatted(ses, 0);
			break;

		case ACT_MAIN_QUIT:
			exit_prog(ses, 1);
			break;

		case ACT_MAIN_REALLY_QUIT:
			exit_prog(ses, 0);
			break;

		case ACT_MAIN_REDRAW:
			redraw_terminal_cls(term);
			break;

		case ACT_MAIN_RELOAD:
			reload(ses, CACHE_MODE_INCREMENT);
			break;

		case ACT_MAIN_RERENDER:
			draw_formatted(ses, 1);
			break;

		case ACT_MAIN_RESET_FORM:
			do_frame_action(ses, reset_form, 0);
			break;

		case ACT_MAIN_RESOURCE_INFO:
			resource_info(term);
			break;

		case ACT_MAIN_RESUME_DOWNLOAD:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				do_frame_action(ses, download_link, action);
			break;

		case ACT_MAIN_SAVE_AS:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				do_frame_action(ses, save_as, 0);
			break;

		case ACT_MAIN_SAVE_FORMATTED:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				do_frame_action(ses, save_formatted_dlg, 0);
			break;

		case ACT_MAIN_SAVE_URL_AS:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				save_url_as(ses);
			break;

		case ACT_MAIN_SAVE_OPTIONS:
			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				write_config(term);
			break;

		case ACT_MAIN_SEARCH:
			do_frame_action(ses, search_dlg, 1);
			break;

		case ACT_MAIN_SEARCH_BACK:
			do_frame_action(ses, search_dlg, -1);
			break;

		case ACT_MAIN_SEARCH_TYPEAHEAD:
		case ACT_MAIN_SEARCH_TYPEAHEAD_LINK:
		case ACT_MAIN_SEARCH_TYPEAHEAD_TEXT:
		case ACT_MAIN_SEARCH_TYPEAHEAD_TEXT_BACK:
			do_frame_action(ses, search_typeahead, action);
			break;

		case ACT_MAIN_SHOW_TERM_OPTIONS:
			terminal_options(term, NULL, ses);
			break;

		case ACT_MAIN_SUBMIT_FORM:
			do_frame_action(ses, submit_form, 0);
			break;

		case ACT_MAIN_SUBMIT_FORM_RELOAD:
			do_frame_action(ses, submit_form, 1);
			break;

		case ACT_MAIN_TAB_NEXT:
			switch_to_next_tab(term);
			break;

		case ACT_MAIN_TAB_MOVE:
			move_current_tab(ses, 1);
			break;

		case ACT_MAIN_TAB_MOVE_BACK:
			move_current_tab(ses, -1);
			break;

		case ACT_MAIN_TAB_MENU:
			assert(ses->tab == get_current_tab(term));

			if (ses->status.show_tabs_bar)
				set_window_ptr(ses->tab, ses->tab->xpos, term->height - 2);
			else
				set_window_ptr(ses->tab, 0, 0);

			tab_menu(term, ses->tab, ses);
			break;

		case ACT_MAIN_TAB_PREV:
			switch_to_prev_tab(term);
			break;

		case ACT_MAIN_TAB_CLOSE:
			close_tab(term, ses);
			break;

		case ACT_MAIN_TAB_CLOSE_ALL_BUT_CURRENT:
			close_all_tabs_but_current(ses);
			break;

		case ACT_MAIN_TOGGLE_CSS:
#ifdef CONFIG_CSS
			toggle_document_option(ses, "document.css.enable");
#endif
			break;

		case ACT_MAIN_TOGGLE_DISPLAY_IMAGES:
			toggle_document_option(ses, "document.browse.images.show_as_links");
			break;

		case ACT_MAIN_TOGGLE_DISPLAY_TABLES:
			toggle_document_option(ses, "document.html.display_tables");
			break;

		case ACT_MAIN_TOGGLE_DOCUMENT_COLORS:
			toggle_document_option(ses, "document.colors.use_document_colors");
			break;

		case ACT_MAIN_TOGGLE_HTML_PLAIN:
			toggle_plain_html(ses, ses->doc_view, 0);
			break;

		case ACT_MAIN_TOGGLE_NUMBERED_LINKS:
			toggle_document_option(ses, "document.browse.links.numbering");
			break;

		case ACT_MAIN_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES:
			toggle_document_option(ses, "document.plain.compress_empty_lines");
			break;

		case ACT_MAIN_TOGGLE_WRAP_TEXT:
			toggle_wrap_text(ses, ses->doc_view, 0);
			break;

		case ACT_MAIN_UNBACK:
			go_unback(ses);
			break;

		case ACT_MAIN_VIEW_IMAGE:
			do_frame_action(ses, view_image, 0);
			break;

		case ACT_MAIN_ZOOM_FRAME:
			do_frame_action(ses, set_frame, 0);
			break;

		case ACT_MAIN_DOWN:
		case ACT_MAIN_EDIT:
		case ACT_MAIN_END:
		case ACT_MAIN_HOME:
		case ACT_MAIN_LEFT:
		case ACT_MAIN_JUMP_TO_LINK:
		case ACT_MAIN_MARK_SET:
		case ACT_MAIN_MARK_GOTO:
		case ACT_MAIN_PAGE_DOWN:
		case ACT_MAIN_PAGE_UP:
		case ACT_MAIN_RIGHT:
		case ACT_MAIN_SCRIPTING_FUNCTION:
		case ACT_MAIN_SCROLL_DOWN:
		case ACT_MAIN_SCROLL_LEFT:
		case ACT_MAIN_SCROLL_RIGHT:
		case ACT_MAIN_SCROLL_UP:
		case ACT_MAIN_UP:
		default:
			if (verbose) {
				INTERNAL("No action handling defined for '%s'.",
					 write_action(KM_MAIN, action));
			}

			return ACT_MAIN_NONE;
	}

	return action;
}
