/* Menu system */
/* $Id: menu.c,v 1.210 2003/12/07 23:34:59 pasky Exp $ */

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
#include "dialogs/mime.h"
#include "dialogs/options.h"
#include "formhist/dialogs.h"
#include "globhist/dialogs.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
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


static void
menu_next_tab(struct terminal *term, void *d, struct session *ses)
{
	switch_to_next_tab(term);
}

static void
menu_prev_tab(struct terminal *term, void *d, struct session *ses)
{
	switch_to_prev_tab(term);
}

static void
menu_close_tab(struct terminal *term, void *d, struct session *ses)
{
	close_tab(term, ses);
}

#if 0
static void
menu_close_other_tabs(struct terminal *term, void *d, struct session *ses)
{
	struct window *current = d;
	int tabs = number_of_tabs(term);
	int pos;

	assert(term && ses && current);
	if_assert_failed return;

	for (pos = tabs - 1; pos >= 0; pos--) {
		struct window *tab = get_tab_by_number(term, pos);

		if (tab != current) delete_window(tab);
	}
}
#endif

void
tab_menu(struct terminal *term, void *d, struct session *ses)
{
	struct menu_item *menu;
	int tabs = number_of_tabs(term);

	assert(term && ses);
	if_assert_failed return;

	menu = new_menu(FREE_LIST);
	if (!menu) return;

	if (tabs > 1) {
		add_to_menu(&menu, N_("Nex~t tab"), ">",
			    (menu_func) menu_next_tab, NULL, 0);

		add_to_menu(&menu, N_("Pre~v tab"), "<",
			    (menu_func) menu_prev_tab, NULL, 0);
	}

	add_to_menu(&menu, N_("~Close tab"), "c",
		    (menu_func) menu_close_tab, NULL, 0);
#if 0
	if (tabs > 1) {
		add_to_menu(&menu, N_("Close ~all but this"), "c",
			    (menu_func) menu_close_other_tabs, d, 0);
	}
#endif

	do_menu(term, menu, ses, 1);
}

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

static inline void
menu_goto_url(struct terminal *term, void *d, struct session *ses)
{
	dialog_goto_url(ses, "");
}

static void dialog_save_url(struct session *ses);

static inline void
menu_save_url_as(struct terminal *term, void *d, struct session *ses)
{
	dialog_save_url(ses);
}

static inline void
menu_go_back(struct terminal *term, void *d, struct session *ses)
{
	go_back(ses);
}

static inline void
menu_reload(struct terminal *term, void *d, struct session *ses)
{
	reload(ses, CACHE_MODE_INCREMENT);
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


static inline void
flush_caches(struct terminal *term, void *d, void *e)
{
	shrink_memory(1);
}


static void
go_historywards(struct terminal *term, struct location *target, struct session *ses)
{
	go_history(ses, target);
}

static struct menu_item no_hist_menu[] = {
	INIT_MENU_ITEM(N_("No history"), M_BAR, NULL, NULL, 0),
	NULL_MENU_ITEM
};

#define history_menu_model(name__, dir__) \
static void \
name__(struct terminal *term, void *ddd, struct session *ses) \
{ \
	struct location *loc; \
	struct menu_item *mi = NULL; \
 \
	if (!have_location(ses)) goto loop_done; \
 \
	for (loc = cur_loc(ses)->dir__; \
	     loc != (struct location *) &ses->history.history; \
	     loc = loc->dir__) { \
		unsigned char *url; \
 \
		if (!mi) { \
			mi = new_menu(FREE_LIST | FREE_TEXT); \
			if (!mi) return; \
		} \
 \
		url = get_no_post_url(loc->vs.url, NULL); \
		if (url) { \
			add_to_menu(&mi, url, "", (menu_func) go_historywards, \
			    	    (void *) loc, NO_INTL); \
		} \
	} \
loop_done: \
 \
	if (!mi) \
		do_menu(term, no_hist_menu, ses, 0); \
	else \
		do_menu(term, mi, ses, 0); \
}

history_menu_model(history_menu, prev);
history_menu_model(unhistory_menu, next);

#undef history_menu_model


static inline void
menu_doc_info(struct terminal *term, void *ddd, struct session *ses)
{
	state_msg(ses);
}

static inline void
menu_header_info(struct terminal *term, void *ddd, struct session *ses)
{
	head_msg(ses);
}

static inline void
menu_toggle_plain_html(struct terminal *term, void *ddd, struct session *ses)
{
	toggle_plain_html(ses, ses->doc_view, 0);
}

static inline void
menu_toggle_images(struct terminal *term, void *ddd, struct session *ses)
{
	toggle_images(ses, ses->doc_view, 0);
}

static inline void
menu_toggle_link_numbering(struct terminal *term, void *ddd, struct session *ses)
{
	toggle_link_numbering(ses, ses->doc_view, 0);
}

static inline void
menu_toggle_document_colors(struct terminal *term, void *ddd, struct session *ses)
{
	toggle_document_colors(ses, ses->doc_view, 0);
}

static void
menu_shell(struct terminal *term, void *xxx, void *yyy)
{
	unsigned char *sh = GETSHELL;

	if (!sh) sh = DEFAULT_SHELL;
	exec_on_terminal(term, sh, "", 1);
}

static inline void
menu_kill_background_connections(struct terminal *term, void *xxx, void *yyy)
{
	abort_background_connections();
}

static struct menu_item file_menu11[] = {
	INIT_MENU_ITEM(N_("Open new ~tab"), "t", open_in_new_tab, (void *) 0, 0),
	INIT_MENU_ITEM(N_("Open new tab in backgroun~d"), "T", open_in_new_tab_in_background,
								(void *) 0, 0),
	INIT_MENU_ITEM(N_("~Go to URL"), "g", menu_goto_url, NULL, 0),
	INIT_MENU_ITEM(N_("Go ~back"), "<-", menu_go_back, NULL, 0),
	INIT_MENU_ITEM(N_("~Reload"), "Ctrl-R", menu_reload, NULL, 0),
	INIT_MENU_ITEM(N_("~History"), M_SUBMENU, history_menu, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("~Unhistory"), M_SUBMENU, unhistory_menu, NULL, SUBMENU),
};

static struct menu_item file_menu21[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("~Save as"), "", save_as, NULL, 0),
	INIT_MENU_ITEM(N_("Save UR~L as"), "", menu_save_url_as, NULL, 0),
	INIT_MENU_ITEM(N_("Save formatted ~document"), "", menu_save_formatted, NULL, 0),
#ifdef BOOKMARKS
	INIT_MENU_ITEM(N_("Bookm~ark document"), "a", launch_bm_add_doc_dialog, NULL, 0),
#endif
};

static struct menu_item file_menu22[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("~Kill background connections"), "", menu_kill_background_connections, NULL, 0),
	INIT_MENU_ITEM(N_("Flush all ~caches"), "", flush_caches, NULL, 0),
	INIT_MENU_ITEM(N_("Resource ~info"), "", res_inf, NULL, 0),
#ifdef LEAK_DEBUG
	INIT_MENU_ITEM(N_("~Memory info"), "", memory_inf, NULL, 0),
#endif
	BAR_MENU_ITEM,
};

static struct menu_item file_menu3[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("E~xit"), "q", exit_prog, NULL, 0),
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
		SET_MENU_ITEM(e, N_("~New window"), o - 1 ? M_SUBMENU : (unsigned char *) "",
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
		SET_MENU_ITEM(e, N_("~OS shell"), "", menu_shell,
			      NULL, 0, 0, HKS_SHOW);
		e++;
		x = 0;
	}

	if (can_resize_window(term->environment)) {
		SET_MENU_ITEM(e, N_("Resize t~erminal"), "", dlg_resize_terminal,
			      NULL, 0, 0, HKS_SHOW);
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
	INIT_MENU_ITEM(N_("~Search"), "/", menu_for_frame, (void *)search_dlg, 0),
	INIT_MENU_ITEM(N_("Search ~backward"), "?", menu_for_frame, (void *)search_back_dlg, 0),
	INIT_MENU_ITEM(N_("Find ~next"), "n", menu_for_frame, (void *)find_next, 0),
	INIT_MENU_ITEM(N_("Find ~previous"), "N", menu_for_frame, (void *)find_next_back, 0),
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("Toggle ~html/plain"), "\\", menu_toggle_plain_html, NULL, 0),
	INIT_MENU_ITEM(N_("Toggle i~mages"), "*", menu_toggle_images, NULL, 0),
	INIT_MENU_ITEM(N_("Toggle ~link numbering"), ".", menu_toggle_link_numbering, NULL, 0),
	INIT_MENU_ITEM(N_("Toggle ~document colors"), "%", menu_toggle_document_colors, NULL, 0),
	INIT_MENU_ITEM(N_("Document ~info"), "=", menu_doc_info, NULL, 0),
	INIT_MENU_ITEM(N_("H~eader info"), "|", menu_header_info, NULL, 0),
	INIT_MENU_ITEM(N_("Frame at ~full-screen"), "f", menu_for_frame, (void *)set_frame, 0),
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("Nex~t tab"), ">", menu_next_tab, NULL, 0),
	INIT_MENU_ITEM(N_("Pre~v tab"), "<", menu_prev_tab, NULL, 0),
	INIT_MENU_ITEM(N_("~Close tab"), "c", menu_close_tab, NULL, 0),
	NULL_MENU_ITEM
};


static struct menu_item help_menu[] = {
	INIT_MENU_ITEM(N_("~ELinks homepage"), "", menu_url_shortcut, ELINKS_HOMEPAGE, 0),
	INIT_MENU_ITEM(N_("~Documentation"), "", menu_url_shortcut, ELINKS_DOC_URL, 0),
	INIT_MENU_ITEM(N_("~Keys"), "", menu_keys, NULL, 0),
	BAR_MENU_ITEM,
#ifdef DEBUG
	INIT_MENU_ITEM(N_("~Bugs information"), "", menu_url_shortcut, ELINKS_BUGS_URL, 0),
	INIT_MENU_ITEM(N_("~ELinks CvsWeb"), "", menu_url_shortcut, ELINKS_CVSWEB_URL, 0),
	BAR_MENU_ITEM,
#endif
	INIT_MENU_ITEM(N_("~Copying"), "", menu_copying, NULL, 0),
	INIT_MENU_ITEM(N_("~About"), "", menu_about, NULL, 0),
	NULL_MENU_ITEM
};


static struct menu_item ext_menu[] = {
	INIT_MENU_ITEM(N_("~Add"), "", menu_add_ext, NULL, 0),
	INIT_MENU_ITEM(N_("~Modify"), M_SUBMENU, menu_list_ext, menu_add_ext, SUBMENU),
	INIT_MENU_ITEM(N_("~Delete"), M_SUBMENU, menu_list_ext, menu_del_ext, SUBMENU),
	NULL_MENU_ITEM
};

static inline void
do_ext_menu(struct terminal *term, void *xxx, struct session *ses)
{
	do_menu(term, ext_menu, ses, 1);
}

static struct menu_item setup_menu[] = {
#ifdef ENABLE_NLS
	INIT_MENU_ITEM(N_("~Language"), M_SUBMENU, menu_language_list, NULL, SUBMENU),
#endif
	INIT_MENU_ITEM(N_("C~haracter set"), M_SUBMENU, charset_list, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("~Terminal options"), "", terminal_options, NULL, 0),
	INIT_MENU_ITEM(N_("File ~extensions"), M_SUBMENU, do_ext_menu, NULL, SUBMENU),
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("~Options manager"), "o", menu_options_manager, NULL, 0),
	INIT_MENU_ITEM(N_("~Keybinding manager"), "k", menu_keybinding_manager, NULL, 0),
	INIT_MENU_ITEM(N_("~Save options"), "", write_config, NULL, 0),
	NULL_MENU_ITEM
};

static struct menu_item setup_menu_anon[] = {
	INIT_MENU_ITEM(N_("~Language"), M_SUBMENU, menu_language_list, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("C~haracter set"), M_SUBMENU, charset_list, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("~Terminal options"), "", terminal_options, NULL, 0),
	NULL_MENU_ITEM
};

static struct menu_item tools_menu[] = {
#ifdef GLOBHIST
	INIT_MENU_ITEM(N_("Global ~history"), "h", menu_history_manager, NULL, 0),
#endif
#ifdef BOOKMARKS
	INIT_MENU_ITEM(N_("~Bookmarks"), "s", menu_bookmark_manager, NULL, 0),
#endif
	INIT_MENU_ITEM(N_("~Cache"), "C", menu_cache_manager, NULL, 0),
	INIT_MENU_ITEM(N_("~Downloads"), "", menu_download_manager, NULL, 0),
#ifdef COOKIES
	INIT_MENU_ITEM(N_("Coo~kies"), "K", menu_cookie_manager, NULL, 0),
#endif
#ifdef FORMS_MEMORY
	INIT_MENU_ITEM(N_("~Form history"), "", menu_formhist_manager, NULL, 0),
#endif
	NULL_MENU_ITEM
};

static void
do_tools_menu(struct terminal *term, void *xxx, struct session *ses)
{
	do_menu(term, tools_menu, ses, 1);
}

static void
do_view_menu(struct terminal *term, void *xxx, struct session *ses)
{
	do_menu(term, view_menu, ses, 1);
}

static void
do_setup_menu(struct terminal *term, void *xxx, struct session *ses)
{
	if (!get_opt_int_tree(cmdline_options, "anonymous"))
		do_menu(term, setup_menu, ses, 1);
	else
		do_menu(term, setup_menu_anon, ses, 1);
}

static inline void
do_help_menu(struct terminal *term, void *xxx, struct session *ses)
{
	do_menu(term, help_menu, ses, 1);
}

static struct menu_item main_menu[] = {
	INIT_MENU_ITEM(N_("~File"), "", do_file_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~View"), "", do_view_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Link"), "", link_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Tools"), "", do_tools_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Setup"), "", do_setup_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Help"), "", do_help_menu, NULL, FREE_LIST | SUBMENU),
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

static void
dialog_save_url(struct session *ses)
{
	input_field(ses->tab->term, NULL, 1,
		    N_("Save URL"), N_("Enter URL"),
		    N_("OK"), N_("Cancel"), ses, &goto_url_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) save_url,
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
