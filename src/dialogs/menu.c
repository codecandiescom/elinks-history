/* Menu system */
/* $Id: menu.c,v 1.263 2004/01/07 02:57:39 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "main.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/menu.h"
#include "bfu/msgbox.h"
#include "bookmarks/dialogs.h"
#include "cache/dialogs.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/options.h"
#include "config/urlhist.h"
#include "cookies/dialogs.h"
#include "dialogs/document.h"
#include "dialogs/download.h"
#include "dialogs/info.h"
#include "dialogs/menu.h"
#include "dialogs/options.h"
#include "formhist/dialogs.h"
#include "globhist/dialogs.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "mime/dialogs.h"
#include "osdep/osdep.h"
#include "osdep/newwin.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/download.h"
#include "sched/event.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/session.h"
#include "sched/task.h"
#include "terminal/kbd.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/link.h"
#include "viewer/text/search.h"
#include "viewer/text/view.h"


/* Helper for url items in help menu. */
static void
menu_url_shortcut(struct terminal *term, void *d, struct session *ses)
{
	unsigned char *u = stracpy((unsigned char *) d);

	if (!u) return;
	goto_url(ses, u);
	mem_free(u);
}

static inline void
menu_for_frame(struct terminal *term,
	       void (*f)(struct session *, struct document_view *, int),
	       struct session *ses)
{
	struct document_view *doc_view;

	assert(ses && f);
	if_assert_failed return;

	if (!have_location(ses)) return;

	doc_view = current_frame(ses);

	assertm(doc_view, "document not formatted");
	if_assert_failed return;

	f(ses, doc_view, 0);
}

void
menu_save_url_as(struct terminal *term, void *d, struct session *ses)
{
	input_field(ses->tab->term, NULL, 1,
		    N_("Save URL"), N_("Enter URL"),
		    N_("OK"), N_("Cancel"), ses, &goto_url_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) save_url,
		    NULL);
}

void
really_exit_prog(struct session *ses)
{
	register_bottom_half((void (*)(void *))destroy_terminal, ses->tab->term);
}

static inline void
dont_exit_prog(struct session *ses)
{
	ses->exit_query = 0;
}

void
query_exit(struct session *ses)
{
	ses->exit_query = 1;
	msg_box(ses->tab->term, NULL, 0,
		N_("Exit ELinks"), AL_CENTER,
		(ses->tab->term->next == ses->tab->term->prev && are_there_downloads())
		? N_("Do you really want to exit ELinks "
		     "(and terminate all downloads)?")
		: N_("Do you really want to exit ELinks?"),
		ses, 2,
		N_("Yes"), (void (*)(void *)) really_exit_prog, B_ENTER,
		N_("No"), (void (*)(void *)) dont_exit_prog, B_ESC);
}

void
exit_prog(struct terminal *term, void *d, struct session *ses)
{
	if (!ses) {
		register_bottom_half((void (*)(void *))destroy_terminal, term);
		return;
	}
	if (!ses->exit_query
	    && (!d || (term->next == term->prev && are_there_downloads()))) {
		query_exit(ses);
		return;
	}
	really_exit_prog(ses);
}


static void
go_historywards(struct terminal *term, struct location *target,
		struct session *ses)
{
	go_history(ses, target);
}

static struct menu_item no_hist_menu[] = {
	INIT_MENU_ITEM(N_("No history"), NULL, ACT_NONE, NULL, NULL, NO_SELECT),
	NULL_MENU_ITEM
};

#define history_menu_model(name__, dir__) 				\
static void 								\
name__(struct terminal *term, void *ddd, struct session *ses) 		\
{ 									\
	struct location *loc; 						\
	struct menu_item *mi = NULL; 					\
 									\
	if (!have_location(ses)) goto loop_done; 			\
 									\
	for (loc = cur_loc(ses)->dir__; 				\
	     loc != (struct location *) &ses->history.history; 		\
	     loc = loc->dir__) { 					\
		unsigned char *url; 					\
 									\
		if (!mi) { 						\
			mi = new_menu(FREE_LIST | FREE_TEXT); 		\
			if (!mi) return; 				\
		} 							\
 									\
		url = get_no_post_url(loc->vs.url, NULL); 		\
		if (url) { 						\
			add_to_menu(&mi, url, NULL, ACT_NONE, 		\
				    (menu_func) go_historywards,	\
			    	    (void *) loc, NO_INTL); 		\
		} 							\
	} 								\
loop_done: 								\
 									\
	if (!mi) 							\
		do_menu(term, no_hist_menu, ses, 0); 			\
	else 								\
		do_menu(term, mi, ses, 0); 				\
}

history_menu_model(history_menu, prev);
history_menu_model(unhistory_menu, next);

#undef history_menu_model


void
menu_shell(struct terminal *term, void *xxx, void *yyy)
{
	unsigned char *sh;

	if (!can_open_os_shell(term->environment)) return;
	
	sh = GETSHELL;
	if (!sh || !*sh) sh = DEFAULT_SHELL;
	if (sh && *sh)
		exec_on_terminal(term, sh, "", 1);
}


void
tab_menu(struct terminal *term, void *d, struct session *ses)
{
	struct menu_item *menu;
	struct window *tab = d;
	int tabs = number_of_tabs(term);
	int i = 0;

	assert(term && ses && tab);
	if_assert_failed return;

	menu = new_menu(FREE_LIST);
	if (!menu) return;

	add_menu_action(&menu, N_("Go ~back"), ACT_BACK, NULL, 0);
	add_menu_action(&menu, N_("Go for~ward"), ACT_UNBACK, NULL, 0);

	add_separator_to_menu(&menu);

#ifdef CONFIG_BOOKMARKS
	add_to_menu(&menu, N_("Bookm~ark document"), NULL, ACT_ADD_BOOKMARK,
		    (menu_func) launch_bm_add_doc_dialog, NULL, 0);
#endif

	add_menu_action(&menu, N_("~Reload"), ACT_RELOAD, NULL, 0);

	if (ses->doc_view && document_has_frames(ses->doc_view->document))
		add_to_menu(&menu, N_("Frame at ~full-screen"), NULL, ACT_ZOOM_FRAME,
			    (menu_func) menu_for_frame, (void *)set_frame, 0);

	/* Keep tab related operations below this separator */
	add_separator_to_menu(&menu);

	if (tabs > 1) {
		add_menu_action(&menu, N_("Nex~t tab"), ACT_TAB_NEXT, NULL, 0);
		add_menu_action(&menu, N_("Pre~v tab"), ACT_TAB_PREV, NULL, 0);
	}

	add_menu_action(&menu, N_("~Close tab"), ACT_TAB_CLOSE, NULL, 0);

	if (tabs > 1) {
		add_menu_action(&menu, N_("C~lose all tabs but the current"),
				ACT_TAB_CLOSE_ALL_BUT_CURRENT, NULL, 0);
#ifdef CONFIG_BOOKMARKS
		add_to_menu(&menu, N_("B~ookmark all tabs"), "", ACT_ADD_BOOKMARK_TABS,
			    (menu_func) menu_bookmark_terminal_tabs, NULL, 0);
#endif
	}

	/* Adjust the menu position taking the menu frame into account */
	while (menu[i].text) i++;
	set_window_ptr(tab, tab->x, int_max(tab->y - i - 1, 0));

	do_menu(term, menu, ses, 1);
}

static void
do_submenu(struct terminal *term, void *menu, struct session *ses)
{
	do_menu(term, menu, ses, 1);
}


static struct menu_item file_menu11[] = {
	INIT_MENU_ITEM(N_("Open new ~tab"), NULL, ACT_OPEN_NEW_TAB, open_in_new_tab, (void *) 0, 0),
	INIT_MENU_ITEM(N_("Open new tab in backgroun~d"), NULL, ACT_OPEN_NEW_TAB_IN_BACKGROUND,
			open_in_new_tab_in_background,(void *) 0, 0),
	INIT_MENU_ITEM(N_("~Go to URL"), NULL, ACT_GOTO_URL, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("Go ~back"), NULL, ACT_BACK, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("Go ~forward"), NULL, ACT_UNBACK, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("~Reload"), NULL, ACT_RELOAD, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("~History"), NULL, ACT_NONE, history_menu, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("~Unhistory"), NULL, ACT_NONE, unhistory_menu, NULL, SUBMENU),
};

static struct menu_item file_menu21[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("~Save as"), NULL, ACT_SAVE_AS, save_as, NULL, 0),
	INIT_MENU_ITEM(N_("Save UR~L as"), NULL, ACT_SAVE_URL_AS, menu_save_url_as, NULL, 0),
	INIT_MENU_ITEM(N_("Sa~ve formatted document"), NULL, ACT_SAVE_FORMATTED,
			menu_save_formatted, NULL, 0),
#ifdef CONFIG_BOOKMARKS
	INIT_MENU_ITEM(N_("Bookm~ark document"), NULL, ACT_ADD_BOOKMARK,
			launch_bm_add_doc_dialog, NULL, 0),
#endif
};

static struct menu_item file_menu22[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("~Kill background connections"), NULL, ACT_KILL_BACKGROUNDED_CONNECTIONS, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("Flush all ~caches"), NULL, ACT_CACHE_MINIMIZE, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("Resource ~info"), NULL, ACT_NONE, res_inf, NULL, 0),
#ifdef LEAK_DEBUG
	INIT_MENU_ITEM(N_("~Memory info"), NULL, ACT_NONE, memory_inf, NULL, 0),
#endif
	BAR_MENU_ITEM,
};

static struct menu_item file_menu3[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("E~xit"), NULL, ACT_QUIT, exit_prog, NULL, 0),
	NULL_MENU_ITEM,
};

static void
do_file_menu(struct terminal *term, void *xxx, struct session *ses)
{
	struct menu_item *file_menu, *e, *f;
	int anonymous = get_opt_int_tree(cmdline_options, "anonymous");
	int x, o;

	file_menu = mem_alloc(sizeof(file_menu11) + sizeof(file_menu21)
			      + sizeof(file_menu22) + sizeof(file_menu3)
			      + 3 * sizeof(struct menu_item));
	if (!file_menu) return;

	e = file_menu;
	o = can_open_in_new(term);
	if (o) {
		SET_MENU_ITEM(e, N_("~New window"), NULL, ACT_OPEN_NEW_WINDOW,
			      (menu_func) open_in_new_window, send_open_new_window,
			      (o - 1) ? SUBMENU : 0, 0, HKS_SHOW);
		e++;
	}

	memcpy(e, file_menu11, sizeof(file_menu11));
	e += sizeof(file_menu11) / sizeof(struct menu_item);

	if (!anonymous) {
		memcpy(e, file_menu21, sizeof(file_menu21));
		e += sizeof(file_menu21) / sizeof(struct menu_item);
	}

	memcpy(e, file_menu22, sizeof(file_menu22));
	e += sizeof(file_menu22) / sizeof(struct menu_item);

	x = 1;
	if (!anonymous && can_open_os_shell(term->environment)) {
		SET_MENU_ITEM(e, N_("~OS shell"), NULL, ACT_OPEN_OS_SHELL,
			      menu_shell, NULL,
			      0, 0, HKS_SHOW);
		e++;
		x = 0;
	}

	if (can_resize_window(term->environment)) {
		SET_MENU_ITEM(e, N_("Resize t~erminal"), NULL, ACT_NONE,
			      dlg_resize_terminal, NULL,
			      0, 0, HKS_SHOW);
		e++;
		x = 0;
	}

	memcpy(e, file_menu3 + x, sizeof(file_menu3) - x * sizeof(struct menu_item));
	e += sizeof(file_menu3) / sizeof(struct menu_item);

	for (f = file_menu; f < e; f++)
		f->flags |= FREE_LIST;

	do_menu(term, file_menu, ses, 1);
}

static struct menu_item view_menu[] = {
	INIT_MENU_ITEM(N_("~Search"), NULL, ACT_SEARCH, menu_for_frame, (void *)search_dlg, 0),
	INIT_MENU_ITEM(N_("Search ~backward"), NULL, ACT_SEARCH_BACK, menu_for_frame, (void *)search_back_dlg, 0),
	INIT_MENU_ITEM(N_("Find ~next"), NULL, ACT_FIND_NEXT, menu_for_frame, (void *)find_next, 0),
	INIT_MENU_ITEM(N_("Find ~previous"), NULL, ACT_FIND_NEXT_BACK, menu_for_frame, (void *)find_next_back, 0),
	INIT_MENU_ITEM(N_("T~ypeahead search"), NULL, ACT_SEARCH_TYPEAHEAD, menu_for_frame, (void *)search_typeahead, 0),
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("Toggle ~html/plain"), NULL, ACT_TOGGLE_HTML_PLAIN, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("Toggle i~mages"), NULL, ACT_TOGGLE_DISPLAY_IMAGES, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("Toggle ~link numbering"), NULL, ACT_TOGGLE_NUMBERED_LINKS, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("Toggle ~document colors"), NULL, ACT_TOGGLE_DOCUMENT_COLORS, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("Document ~info"), NULL, ACT_DOCUMENT_INFO, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("H~eader info"), NULL, ACT_HEADER_INFO, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("Frame at ~full-screen"), NULL, ACT_ZOOM_FRAME, menu_for_frame, (void *)set_frame, 0),
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("Nex~t tab"), NULL, ACT_TAB_NEXT, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("Pre~v tab"), NULL, ACT_TAB_PREV, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("~Close tab"), NULL, ACT_TAB_CLOSE, NULL, NULL, 0),
	NULL_MENU_ITEM
};


static struct menu_item help_menu[] = {
	INIT_MENU_ITEM(N_("~ELinks homepage"), NULL, ACT_NONE, menu_url_shortcut, ELINKS_HOMEPAGE, 0),
	INIT_MENU_ITEM(N_("~Documentation"), NULL, ACT_NONE, menu_url_shortcut, ELINKS_DOC_URL, 0),
	INIT_MENU_ITEM(N_("~Keys"), NULL, ACT_NONE, menu_keys, NULL, 0),
	BAR_MENU_ITEM,
#ifdef DEBUG
	INIT_MENU_ITEM(N_("~Bugs information"), NULL, ACT_NONE, menu_url_shortcut, ELINKS_BUGS_URL, 0),
	INIT_MENU_ITEM(N_("ELinks C~vsWeb"), NULL, ACT_NONE, menu_url_shortcut, ELINKS_CVSWEB_URL, 0),
	BAR_MENU_ITEM,
#endif
	INIT_MENU_ITEM(N_("~Copying"), NULL, ACT_NONE, menu_copying, NULL, 0),
	INIT_MENU_ITEM(N_("~About"), NULL, ACT_NONE, menu_about, NULL, 0),
	NULL_MENU_ITEM
};


static struct menu_item ext_menu[] = {
	INIT_MENU_ITEM(N_("~Add"), NULL, ACT_NONE, menu_add_ext, NULL, 0),
	INIT_MENU_ITEM(N_("~Modify"), NULL, ACT_NONE, menu_list_ext, menu_add_ext, SUBMENU),
	INIT_MENU_ITEM(N_("~Delete"), NULL, ACT_NONE, menu_list_ext, menu_del_ext, SUBMENU),
	NULL_MENU_ITEM
};

static struct menu_item setup_menu[] = {
#ifdef ENABLE_NLS
	INIT_MENU_ITEM(N_("~Language"), NULL, ACT_NONE, menu_language_list, NULL, SUBMENU),
#endif
	INIT_MENU_ITEM(N_("C~haracter set"), NULL, ACT_NONE, charset_list, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("~Terminal options"), NULL, ACT_SHOW_TERM_OPTIONS, terminal_options, NULL, 0),
	INIT_MENU_ITEM(N_("File ~extensions"), NULL, ACT_NONE, do_submenu, ext_menu, SUBMENU),
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("~Options manager"), NULL, ACT_OPTIONS_MANAGER, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("~Keybinding manager"), NULL, ACT_KEYBINDING_MANAGER, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("~Save options"), NULL, ACT_SAVE_OPTIONS, write_config, NULL, 0),
	NULL_MENU_ITEM
};

static struct menu_item setup_menu_anon[] = {
	INIT_MENU_ITEM(N_("~Language"), NULL, ACT_NONE, menu_language_list, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("C~haracter set"), NULL, ACT_NONE, charset_list, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("~Terminal options"), NULL, ACT_NONE, terminal_options, NULL, 0),
	NULL_MENU_ITEM
};

static struct menu_item tools_menu[] = {
#ifdef CONFIG_GLOBHIST
	INIT_MENU_ITEM(N_("Global ~history"), NULL, ACT_HISTORY_MANAGER, NULL, NULL, 0),
#endif
#ifdef CONFIG_BOOKMARKS
	INIT_MENU_ITEM(N_("~Bookmarks"), NULL, ACT_BOOKMARK_MANAGER, NULL, NULL, 0),
#endif
	INIT_MENU_ITEM(N_("~Cache"), NULL, ACT_CACHE_MANAGER, NULL, NULL, 0),
	INIT_MENU_ITEM(N_("~Downloads"), NULL, ACT_DOWNLOAD_MANAGER, NULL, NULL, 0),
#ifdef CONFIG_COOKIES
	INIT_MENU_ITEM(N_("Coo~kies"), NULL, ACT_COOKIE_MANAGER, NULL, NULL, 0),
#endif
#ifdef CONFIG_FORMHIST
	INIT_MENU_ITEM(N_("~Form history"), NULL, ACT_FORMHIST_MANAGER, NULL, NULL, 0),
#endif
	NULL_MENU_ITEM
};

static void
do_setup_menu(struct terminal *term, void *xxx, struct session *ses)
{
	if (!get_opt_int_tree(cmdline_options, "anonymous"))
		do_menu(term, setup_menu, ses, 1);
	else
		do_menu(term, setup_menu_anon, ses, 1);
}

static struct menu_item main_menu[] = {
	INIT_MENU_ITEM(N_("~File"), NULL, ACT_NONE, do_file_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~View"), NULL, ACT_NONE, do_submenu, view_menu, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Link"), NULL, ACT_NONE, link_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Tools"), NULL, ACT_NONE, do_submenu, tools_menu, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Setup"), NULL, ACT_NONE, do_setup_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Help"), NULL, ACT_NONE, do_submenu, help_menu, FREE_LIST | SUBMENU),
	NULL_MENU_ITEM
};

void
activate_bfu_technology(struct session *ses, int item)
{
	do_mainmenu(ses->tab->term, main_menu, ses, item);
}


void
dialog_goto_url(struct session *ses, char *url)
{
	input_field(ses->tab->term, NULL, 1,
		    N_("Go to URL"), N_("Enter URL"),
		    N_("OK"), N_("Cancel"), ses, &goto_url_history,
		    MAX_STR_LEN, url, 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) goto_url_with_hook,
		    NULL);
}

static struct input_history file_history = {
	/* items: */	{ D_LIST_HEAD(file_history.entries) },
	/* size: */	0,
	/* dirty: */	0,
	/* nosave: */	0,
};

void
query_file(struct session *ses, unsigned char *url, void *data,
	   void (*std)(void *, unsigned char *),
	   void (*cancel)(void *), int interactive)
{
	struct string def;

	if (!init_string(&def)) return;

	add_to_string(&def, get_opt_str("document.download.directory"));
	if (def.length && !dir_sep(def.source[def.length - 1]))
		add_char_to_string(&def, '/');

	add_string_uri_filename_to_string(&def, url);

	if (interactive) {
		input_field(ses->tab->term, NULL, 1,
			    N_("Download"), N_("Save to file"),
			    N_("OK"),  N_("Cancel"), data, &file_history,
			    MAX_STR_LEN, def.source, 0, 0, NULL,
			    (void (*)(void *, unsigned char *)) std,
			    (void (*)(void *)) cancel);
	} else {
		std(data, def.source);
	}

	done_string(&def);
}

void
free_history_lists(void)
{
	free_list(goto_url_history.entries);
	free_list(file_history.entries);
#ifdef HAVE_SCRIPTING
	trigger_event_name("free-history");
#endif
}
