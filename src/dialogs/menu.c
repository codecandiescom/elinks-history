/* Menu system */
/* $Id: menu.c,v 1.300 2004/04/15 16:57:18 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/menu.h"
#include "bfu/msgbox.h"
#include "config/options.h"
#include "config/urlhist.h"
#include "document/document.h"
#include "document/view.h"
#include "dialogs/exmode.h"
#include "dialogs/info.h"
#include "dialogs/menu.h"
#include "dialogs/options.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "main.h"
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
#include "util/conv.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/link.h"
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

void
save_url_as(struct session *ses)
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
exit_prog(struct session *ses, int query)
{
	assert(ses);

	if (!ses->exit_query
	    && (query || (ses->tab->term->next == ses->tab->term->prev
			  && are_there_downloads()))) {
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
	INIT_MENU_ITEM(N_("No history"), NULL, ACT_MAIN_NONE, NULL, NULL, NO_SELECT),
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
		url = get_uri_string(loc->vs.uri, URI_PUBLIC); 		\
		if (url) { 						\
			add_to_menu(&mi, url, NULL, ACT_MAIN_NONE, 		\
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
tab_menu(struct terminal *term, void *d, struct session *ses)
{
	struct menu_item *menu;
	struct window *tab = d;
	int tabs = number_of_tabs(term);
	int i = 0;
#ifdef CONFIG_BOOKMARKS
	int anonymous = get_opt_bool_tree(cmdline_options, "anonymous");
#endif

	assert(term && ses && tab);
	if_assert_failed return;

	menu = new_menu(FREE_LIST);
	if (!menu) return;

	add_menu_action(&menu, N_("Go ~back"), ACT_MAIN_BACK);
	add_menu_action(&menu, N_("Go for~ward"), ACT_MAIN_UNBACK);

	add_separator_to_menu(&menu);

#ifdef CONFIG_BOOKMARKS
	if (!anonymous) {
		add_menu_action(&menu, N_("Bookm~ark document"), ACT_MAIN_ADD_BOOKMARK);
	}
#endif

	add_menu_action(&menu, N_("~Reload"), ACT_MAIN_RELOAD);

	if (ses->doc_view && document_has_frames(ses->doc_view->document))
		add_menu_action(&menu, N_("Frame at ~full-screen"), ACT_MAIN_ZOOM_FRAME);

	/* Keep tab related operations below this separator */
	add_separator_to_menu(&menu);

	if (tabs > 1) {
		add_menu_action(&menu, N_("Nex~t tab"), ACT_MAIN_TAB_NEXT);
		add_menu_action(&menu, N_("Pre~v tab"), ACT_MAIN_TAB_PREV);
	}

	add_menu_action(&menu, N_("~Close tab"), ACT_MAIN_TAB_CLOSE);

	if (tabs > 1) {
		add_menu_action(&menu, N_("C~lose all tabs but the current"),
				ACT_MAIN_TAB_CLOSE_ALL_BUT_CURRENT);
#ifdef CONFIG_BOOKMARKS
		if (!anonymous) {
			add_menu_action(&menu, N_("B~ookmark all tabs"),
					ACT_MAIN_ADD_BOOKMARK_TABS);
		}
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
	INIT_MENU_ACTION(N_("Open new ~tab"), ACT_MAIN_OPEN_NEW_TAB),
	INIT_MENU_ACTION(N_("Open new tab in backgroun~d"), ACT_MAIN_OPEN_NEW_TAB_IN_BACKGROUND),
	INIT_MENU_ACTION(N_("~Go to URL"), ACT_MAIN_GOTO_URL),
	INIT_MENU_ACTION(N_("Go ~back"), ACT_MAIN_BACK),
	INIT_MENU_ACTION(N_("Go ~forward"), ACT_MAIN_UNBACK),
	INIT_MENU_ITEM(N_("~History"), NULL, ACT_MAIN_NONE, history_menu, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("~Unhistory"), NULL, ACT_MAIN_NONE, unhistory_menu, NULL, SUBMENU),
};

static struct menu_item file_menu21[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("~Save as"), ACT_MAIN_SAVE_AS),
	INIT_MENU_ACTION(N_("Save UR~L as"), ACT_MAIN_SAVE_URL_AS),
	INIT_MENU_ACTION(N_("Sa~ve formatted document"), ACT_MAIN_SAVE_FORMATTED),
#ifdef CONFIG_BOOKMARKS
	INIT_MENU_ACTION(N_("Bookm~ark document"), ACT_MAIN_ADD_BOOKMARK),
#endif
};

static struct menu_item file_menu22[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("~Kill background connections"), ACT_MAIN_KILL_BACKGROUNDED_CONNECTIONS),
	INIT_MENU_ACTION(N_("Flush all ~caches"), ACT_MAIN_CACHE_MINIMIZE),
	INIT_MENU_ACTION(N_("Resource ~info"), ACT_MAIN_RESOURCE_INFO),
#ifdef LEAK_DEBUG
	INIT_MENU_ITEM(N_("~Memory info"), NULL, ACT_MAIN_NONE, memory_inf, NULL, 0),
#endif
	BAR_MENU_ITEM,
};

static struct menu_item file_menu3[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("E~xit"), ACT_MAIN_QUIT),
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
		SET_MENU_ITEM(e, N_("~New window"), NULL, ACT_MAIN_OPEN_NEW_WINDOW,
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
		SET_MENU_ITEM(e, N_("~OS shell"), NULL, ACT_MAIN_OPEN_OS_SHELL,
			      NULL, NULL, 0, 0, HKS_SHOW);
		e++;
		x = 0;
	}

	if (can_resize_window(term->environment)) {
		SET_MENU_ITEM(e, N_("Resize t~erminal"), NULL, ACT_MAIN_NONE,
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
	INIT_MENU_ACTION(N_("~Search"), ACT_MAIN_SEARCH),
	INIT_MENU_ACTION(N_("Search ~backward"), ACT_MAIN_SEARCH_BACK),
	INIT_MENU_ACTION(N_("Find ~next"), ACT_MAIN_FIND_NEXT),
	INIT_MENU_ACTION(N_("Find ~previous"), ACT_MAIN_FIND_NEXT_BACK),
	INIT_MENU_ACTION(N_("T~ypeahead search"), ACT_MAIN_SEARCH_TYPEAHEAD),
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("Toggle ~html/plain"), ACT_MAIN_TOGGLE_HTML_PLAIN),
	INIT_MENU_ACTION(N_("Toggle i~mages"), ACT_MAIN_TOGGLE_DISPLAY_IMAGES),
	INIT_MENU_ACTION(N_("Toggle ~link numbering"), ACT_MAIN_TOGGLE_NUMBERED_LINKS),
	INIT_MENU_ACTION(N_("Toggle ~document colors"), ACT_MAIN_TOGGLE_DOCUMENT_COLORS),
	INIT_MENU_ACTION(N_("~Wrap text on/off"), ACT_MAIN_TOGGLE_WRAP_TEXT),
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("Document ~info"), ACT_MAIN_DOCUMENT_INFO),
	INIT_MENU_ACTION(N_("H~eader info"), ACT_MAIN_HEADER_INFO),
	INIT_MENU_ACTION(N_("Rel~oad document"), ACT_MAIN_RELOAD),
	INIT_MENU_ACTION(N_("~Rerender document"), ACT_MAIN_RERENDER),
	INIT_MENU_ACTION(N_("Frame at ~full-screen"), ACT_MAIN_ZOOM_FRAME),
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("Nex~t tab"), ACT_MAIN_TAB_NEXT),
	INIT_MENU_ACTION(N_("Pre~v tab"), ACT_MAIN_TAB_PREV),
	INIT_MENU_ACTION(N_("~Close tab"), ACT_MAIN_TAB_CLOSE),
	NULL_MENU_ITEM
};


static struct menu_item help_menu[] = {
	INIT_MENU_ITEM(N_("~ELinks homepage"), NULL, ACT_MAIN_NONE, menu_url_shortcut, ELINKS_HOMEPAGE, 0),
	INIT_MENU_ITEM(N_("~Documentation"), NULL, ACT_MAIN_NONE, menu_url_shortcut, ELINKS_DOC_URL, 0),
	INIT_MENU_ITEM(N_("~Keys"), NULL, ACT_MAIN_NONE, menu_keys, NULL, 0),
	BAR_MENU_ITEM,
#ifdef CONFIG_DEBUG
	INIT_MENU_ITEM(N_("~Bugs information"), NULL, ACT_MAIN_NONE, menu_url_shortcut, ELINKS_BUGS_URL, 0),
	INIT_MENU_ITEM(N_("ELinks C~vsWeb"), NULL, ACT_MAIN_NONE, menu_url_shortcut, ELINKS_CVSWEB_URL, 0),
	BAR_MENU_ITEM,
#endif
	INIT_MENU_ITEM(N_("~Copying"), NULL, ACT_MAIN_NONE, menu_copying, NULL, 0),
	INIT_MENU_ITEM(N_("~About"), NULL, ACT_MAIN_NONE, menu_about, NULL, 0),
	NULL_MENU_ITEM
};


static struct menu_item ext_menu[] = {
	INIT_MENU_ITEM(N_("~Add"), NULL, ACT_MAIN_NONE, menu_add_ext, NULL, 0),
	INIT_MENU_ITEM(N_("~Modify"), NULL, ACT_MAIN_NONE, menu_list_ext, menu_add_ext, SUBMENU),
	INIT_MENU_ITEM(N_("~Delete"), NULL, ACT_MAIN_NONE, menu_list_ext, menu_del_ext, SUBMENU),
	NULL_MENU_ITEM
};

static struct menu_item setup_menu[] = {
#ifdef ENABLE_NLS
	INIT_MENU_ITEM(N_("~Language"), NULL, ACT_MAIN_NONE, menu_language_list, NULL, SUBMENU),
#endif
	INIT_MENU_ITEM(N_("C~haracter set"), NULL, ACT_MAIN_NONE, charset_list, NULL, SUBMENU),
	INIT_MENU_ACTION(N_("~Terminal options"), ACT_MAIN_SHOW_TERM_OPTIONS),
	INIT_MENU_ITEM(N_("File ~extensions"), NULL, ACT_MAIN_NONE, do_submenu, ext_menu, SUBMENU),
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("~Options manager"), ACT_MAIN_OPTIONS_MANAGER),
	INIT_MENU_ACTION(N_("~Keybinding manager"), ACT_MAIN_KEYBINDING_MANAGER),
	INIT_MENU_ACTION(N_("~Save options"), ACT_MAIN_SAVE_OPTIONS),
	NULL_MENU_ITEM
};

static struct menu_item setup_menu_anon[] = {
	INIT_MENU_ITEM(N_("~Language"), NULL, ACT_MAIN_NONE, menu_language_list, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("C~haracter set"), NULL, ACT_MAIN_NONE, charset_list, NULL, SUBMENU),
	INIT_MENU_ACTION(N_("~Terminal options"), ACT_MAIN_SHOW_TERM_OPTIONS),
	NULL_MENU_ITEM
};

static struct menu_item tools_menu[] = {
#ifdef CONFIG_GLOBHIST
	INIT_MENU_ACTION(N_("Global ~history"), ACT_MAIN_HISTORY_MANAGER),
#endif
#ifdef CONFIG_BOOKMARKS
	INIT_MENU_ACTION(N_("~Bookmarks"), ACT_MAIN_BOOKMARK_MANAGER),
#endif
	INIT_MENU_ACTION(N_("~Cache"), ACT_MAIN_CACHE_MANAGER),
	INIT_MENU_ACTION(N_("~Downloads"), ACT_MAIN_DOWNLOAD_MANAGER),
#ifdef CONFIG_COOKIES
	INIT_MENU_ACTION(N_("Coo~kies"), ACT_MAIN_COOKIE_MANAGER),
#endif
#ifdef CONFIG_FORMHIST
	INIT_MENU_ACTION(N_("~Form history"), ACT_MAIN_FORMHIST_MANAGER),
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
	INIT_MENU_ITEM(N_("~File"), NULL, ACT_MAIN_NONE, do_file_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~View"), NULL, ACT_MAIN_NONE, do_submenu, view_menu, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Link"), NULL, ACT_MAIN_NONE, link_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Tools"), NULL, ACT_MAIN_NONE, do_submenu, tools_menu, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Setup"), NULL, ACT_MAIN_NONE, do_setup_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Help"), NULL, ACT_MAIN_NONE, do_submenu, help_menu, FREE_LIST | SUBMENU),
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


static INIT_INPUT_HISTORY(file_history);

void
query_file(struct session *ses, struct uri *uri, void *data,
	   void (*std)(void *, unsigned char *),
	   void (*cancel)(void *), int interactive)
{
	struct string def;

	if (!init_string(&def)) return;

	add_to_string(&def, get_opt_str("document.download.directory"));
	if (def.length && !dir_sep(def.source[def.length - 1]))
		add_char_to_string(&def, '/');

	add_uri_filename_to_string(&def, uri);

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
#ifdef CONFIG_EXMODE
	free_list(exmode_history.entries);
#endif
#ifdef HAVE_SCRIPTING
	trigger_event_name("free-history");
#endif
}


static struct string *
init_session_info_string(struct string *parameters, struct session *ses)
{
	int ring = get_opt_int_tree(cmdline_options, "session-ring");

	if (!init_string(parameters)) return NULL;

	add_format_to_string(parameters, "-base-session %d ", ses->id);
	if (ring) add_format_to_string(parameters, " -session-ring %d ", ring);

	return parameters;
}


void
open_url_in_new_window(struct session *ses, unsigned char *url,
			void (*open_window)(struct terminal *, unsigned char *, unsigned char *))
{
	struct string parameters;

	assert(open_window && ses);
	if_assert_failed return;

	if (!init_session_info_string(&parameters, ses)) return;

	/* TODO: Possibly preload the link URI so it will be ready when
	 * the new ELinks instance requests it. --jonas */
	if (url) add_encoded_shell_safe_url(&parameters, url);

	open_window(ses->tab->term, path_to_exe, parameters.source);
	done_string(&parameters);
}

/* open a link in a new xterm */
void
send_open_in_new_window(struct terminal *term,
		       void (*open_window)(struct terminal *term, unsigned char *, unsigned char *),
		       struct session *ses)
{
	struct document_view *doc_view;
	struct link *link;
	unsigned char *url;

	assert(term && open_window && ses);
	if_assert_failed return;
	doc_view = current_frame(ses);
	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	link = get_current_link(doc_view);
	if (!link) return;

	url = get_link_url(ses, doc_view, link);
	if (!url) return;

	open_url_in_new_window(ses, url, open_window);
	mem_free(url);
}

void
send_open_new_window(struct terminal *term,
		    void (*open_window)(struct terminal *, unsigned char *, unsigned char *),
		    struct session *ses)
{
	open_url_in_new_window(ses, NULL, open_window);
}


void
open_in_new_window(struct terminal *term,
		   void (*xxx)(struct terminal *,
			       void (*)(struct terminal *, unsigned char *, unsigned char *),
			       struct session *ses),
		   struct session *ses)
{
	struct menu_item *mi;
	struct open_in_new *oi, *oin;

	assert(term && ses && xxx);
	if_assert_failed return;

	oin = get_open_in_new(term);
	if (!oin) return;
	if (!oin[1].text) {
		xxx(term, oin[0].fn, ses);
		mem_free(oin);
		return;
	}

	mi = new_menu(FREE_LIST);
	if (!mi) {
		mem_free(oin);
		return;
	}
	for (oi = oin; oi->text; oi++)
		add_to_menu(&mi, oi->text, NULL, ACT_MAIN_NONE, (menu_func) xxx, oi->fn, 0);
	mem_free(oin);
	do_menu(term, mi, ses, 1);
}


void
add_new_win_to_menu(struct menu_item **mi, unsigned char *text, int action,
		    struct terminal *term)
{
	int c = can_open_in_new(term);

	if (!c) return;
	add_to_menu(mi, text, NULL, action, (menu_func) open_in_new_window,
		    send_open_in_new_window, c - 1 ? SUBMENU : 0);
}
