/* Sessions action management */
/* $Id: action.c,v 1.2 2004/01/07 01:57:17 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "main.h"

#include "cache/cache.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "dialogs/document.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/view.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "sched/action.h"
#include "sched/connection.h"
#include "sched/event.h"
#include "sched/session.h"
#include "sched/task.h"
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


/* This could gradually become some mulitplexor / switch noodle containing
 * most if not all default handling of actions (for the main mapping) that
 * frame_ev() and/or send_event() could use as a backend. */
enum keyact
do_action(struct session *ses, enum keyact action, void *data, int verbose)
{
	struct terminal *term = ses->tab->term;

	switch (action) {
		/* Please keep in alphabetical order for now. Later we can sort
		 * by most used or something. */
		case ACT_BACK:
			go_back(ses);
			break;

		case ACT_CACHE_MINIMIZE:
			shrink_memory(1);
			break;

		case ACT_DOCUMENT_INFO:
			state_msg(ses);
			break;

		case ACT_GOTO_URL:
			dialog_goto_url(ses, "");
			break;

		case ACT_HEADER_INFO:
			head_msg(ses);
			break;

		case ACT_KILL_BACKGROUNDED_CONNECTIONS:
			abort_background_connections();
			break;

		case ACT_RELOAD:
			reload(ses, CACHE_MODE_INCREMENT);
			break;

		case ACT_TAB_NEXT:
			switch_to_next_tab(term);
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

		case ACT_TOGGLE_DOCUMENT_COLORS:
			toggle_document_option(ses, "document.colors.use_document_colors");
			break;

		case ACT_TOGGLE_HTML_PLAIN:
			toggle_plain_html(ses, ses->doc_view, 0);
			break;

		case ACT_TOGGLE_NUMBERED_LINKS:
			toggle_document_option(ses, "document.browse.links.numbering");
			break;

		case ACT_UNBACK:
			go_unback(ses);
			break;

		case ACT_ABORT_CONNECTION:
		case ACT_ADD_BOOKMARK:
		case ACT_ADD_BOOKMARK_TABS:
		case ACT_ADD_BOOKMARK_LINK:
		case ACT_AUTO_COMPLETE:
		case ACT_AUTO_COMPLETE_UNAMBIGUOUS:
		case ACT_BACKSPACE:
		case ACT_BEGINNING_OF_BUFFER:
		case ACT_BOOKMARK_MANAGER:
		case ACT_CACHE_MANAGER:
		case ACT_CANCEL:
		case ACT_COOKIE_MANAGER:
		case ACT_COOKIES_LOAD:
		case ACT_COPY_CLIPBOARD:
		case ACT_CUT_CLIPBOARD:
		case ACT_DELETE:
		case ACT_DOWN:
		case ACT_DOWNLOAD:
		case ACT_DOWNLOAD_IMAGE:
		case ACT_DOWNLOAD_MANAGER:
		case ACT_EDIT:
		case ACT_END:
		case ACT_END_OF_BUFFER:
		case ACT_ENTER:
		case ACT_ENTER_RELOAD:
		case ACT_EXPAND:
		case ACT_FILE_MENU:
		case ACT_FIND_NEXT:
		case ACT_FIND_NEXT_BACK:
		case ACT_FORGET_CREDENTIALS:
		case ACT_FORMHIST_MANAGER:
		case ACT_GOTO_URL_CURRENT:
		case ACT_GOTO_URL_CURRENT_LINK:
		case ACT_GOTO_URL_HOME:
		case ACT_HISTORY_MANAGER:
		case ACT_HOME:
		case ACT_KILL_TO_BOL:
		case ACT_KILL_TO_EOL:
		case ACT_KEYBINDING_MANAGER:
		case ACT_LEFT:
		case ACT_LINK_MENU:
		case ACT_JUMP_TO_LINK:
		case ACT_LUA_CONSOLE:
		case ACT_MARK_SET:
		case ACT_MARK_GOTO:
		case ACT_MARK_ITEM:
		case ACT_MENU:
		case ACT_NEXT_FRAME:
		case ACT_NEXT_ITEM:
		case ACT_OPEN_NEW_TAB:
		case ACT_OPEN_NEW_TAB_IN_BACKGROUND:
		case ACT_OPEN_NEW_WINDOW:
		case ACT_OPEN_LINK_IN_NEW_TAB:
		case ACT_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND:
		case ACT_OPEN_LINK_IN_NEW_WINDOW:
		case ACT_OPEN_OS_SHELL:
		case ACT_OPTIONS_MANAGER:
		case ACT_PAGE_DOWN:
		case ACT_PAGE_UP:
		case ACT_PASTE_CLIPBOARD:
		case ACT_PREVIOUS_FRAME:
		case ACT_QUIT:
		case ACT_REALLY_QUIT:
		case ACT_REDRAW:
		case ACT_RESUME_DOWNLOAD:
		case ACT_RIGHT:
		case ACT_SAVE_AS:
		case ACT_SAVE_URL_AS:
		case ACT_SAVE_FORMATTED:
		case ACT_SAVE_OPTIONS:
		case ACT_SCRIPTING_FUNCTION:
		case ACT_SCROLL_DOWN:
		case ACT_SCROLL_LEFT:
		case ACT_SCROLL_RIGHT:
		case ACT_SCROLL_UP:
		case ACT_SEARCH:
		case ACT_SEARCH_BACK:
		case ACT_SEARCH_TYPEAHEAD:
		case ACT_SELECT:
		case ACT_SHOW_TERM_OPTIONS:
		case ACT_TAB_MENU:
		case ACT_TOGGLE_DISPLAY_TABLES:
		case ACT_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES:
		case ACT_UNEXPAND:
		case ACT_UP:
		case ACT_VIEW_IMAGE:
		case ACT_ZOOM_FRAME:
		default:
			if (verbose) {
				INTERNAL("No action handling defined for '%s'.",
					 write_action(action));
			}

			return ACT_NONE;
	}

	return action;
}
