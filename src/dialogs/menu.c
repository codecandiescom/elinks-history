/* Menu system */
/* $Id: menu.c,v 1.37 2002/07/05 16:19:27 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "main.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/menu.h"
#include "bfu/msgbox.h"
#include "bookmarks/dialogs.h"
#include "config/conf.h"
#include "config/options.h"
#include "dialogs/document.h"
#include "dialogs/globhist.h"
#include "dialogs/info.h"
#include "dialogs/menu.h"
#include "dialogs/options.h"
#include "document/download.h"
#include "document/history.h"
#include "document/location.h"
#include "document/session.h"
#include "document/view.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/sched.h"
#include "lowlevel/select.h"
#include "lowlevel/terminal.h"
#include "lua/core.h"
#include "lua/hooks.h"
#include "protocol/types.h"
#include "protocol/url.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"


void
menu_manual(struct terminal *term, void *d, struct session *ses)
{
	goto_url(ses, LINKS_MANUAL_URL);
}

void
menu_for_frame(struct terminal *term,
	       void (*f)(struct session *, struct f_data_c *, int),
	       struct session *ses)
{
	do_for_frame(ses, f, 0);
}

void
menu_goto_url(struct terminal *term, void *d, struct session *ses)
{
	dialog_goto_url(ses, "");
}

void dialog_save_url(struct session *ses);

void
menu_save_url_as(struct terminal *term, void *d, struct session *ses)
{
	dialog_save_url(ses);
}

void
menu_go_back(struct terminal *term, void *d, struct session *ses)
{
	go_back(ses);
}

void
menu_reload(struct terminal *term, void *d, struct session *ses)
{
	reload(ses, -1);
}

void
really_exit_prog(struct session *ses)
{
	register_bottom_half((void (*)(void *))destroy_terminal, ses->term);
}

void
dont_exit_prog(struct session *ses)
{
	ses->exit_query = 0;
}

void
query_exit(struct session *ses)
{
	ses->exit_query = 1;
	msg_box(ses->term, NULL,
		TEXT(T_EXIT_LINKS), AL_CENTER,
		(ses->term->next == ses->term->prev && are_there_downloads())
		? TEXT(T_DO_YOU_REALLY_WANT_TO_EXIT_LINKS_AND_TERMINATE_ALL_DOWNLOADS)
		: TEXT(T_DO_YOU_REALLY_WANT_TO_EXIT_LINKS),
		ses, 2,
		TEXT(T_YES), (void (*)(void *)) really_exit_prog, B_ENTER,
		TEXT(T_NO), (void (*)(void *)) dont_exit_prog, B_ESC);
}

void
exit_prog(struct terminal *term, void *d, struct session *ses)
{
	if (!ses) {
		register_bottom_half((void (*)(void *))destroy_terminal, term);
		return;
	}
	if (!ses->exit_query && (!d || (term->next == term->prev && are_there_downloads()))) {
		query_exit(ses);
		return;
	}
	really_exit_prog(ses);
}


void
flush_caches(struct terminal *term, void *d, void *e)
{
	shrink_memory(1);
}

void
go_backwards(struct terminal *term, void *psteps, struct session *ses)
{
	int steps = (int) psteps;

#if 0
	if (ses->tq_goto_position)
		--steps;
	if (ses->search_word)
		mem_free(ses->search_word), ses->search_word = NULL;
#endif

	abort_loading(ses);

	/* Move all intermediate items to unhistory... */

	while (steps-- > 1) {
		struct location *loc = ses->history.next;

		if ((void *) loc == &ses->history) return;

		/* First item in history/unhistory is something special and
		 * precious... like... like... the current location? */

		loc = loc->next;
		if ((void *) loc == &ses->history) return;

		del_from_list(loc);
		add_to_list(ses->unhistory, loc);
	}

	/* ..and now go back in history by one as usual. */

	if (steps >= 0) /* => psteps >= 1 */
		go_back(ses);
}

void
go_unbackwards(struct terminal *term, void *psteps, struct session *ses)
{
	int steps = (int) psteps;

	abort_loading(ses);

	/* Move all intermediate items to history... */

	while (steps--) {
	    	struct location *loc = ses->unhistory.next;

		if ((void *) loc == &ses->unhistory) return;

		del_from_list(loc);
		/* Skip the first entry, which is current location. */
		add_to_list(cur_loc(ses)->next, loc);
	}

	/* ..and now go unback in unhistory by one as usual. */

	go_unback(ses);
}

struct menu_item no_hist_menu[] = {
	{TEXT(T_NO_HISTORY), "", M_BAR, NULL, NULL, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

void
history_menu(struct terminal *term, void *ddd, struct session *ses)
{
	struct location *l;
	struct menu_item *mi = NULL;
	int n = 0;

	foreach(l, ses->history) {
		if (n/* || ses->tq_goto_position*/) {
			unsigned char *url;
			if (!mi && !(mi = new_menu(3))) return;
			url = stracpy(l->vs.url);
			if (strchr(url, POST_CHAR)) *strchr(url, POST_CHAR) = 0;
			add_to_menu(&mi, url, "", "", MENU_FUNC go_backwards, (void *) n, 0);
		}
		n++;
	}
	if (n <= 1) do_menu(term, no_hist_menu, ses);
	else do_menu(term, mi, ses);
}

void
unhistory_menu(struct terminal *term, void *ddd, struct session *ses)
{
	struct location *l;
	struct menu_item *mi = NULL;
	int n = 0;

	foreach(l, ses->unhistory) {
		unsigned char *url;
		if (!mi && !(mi = new_menu(3))) return;
		url = stracpy(l->vs.url);
		if (strchr(url, POST_CHAR)) *strchr(url, POST_CHAR) = 0;
		add_to_menu(&mi, url, "", "", MENU_FUNC go_unbackwards, (void *) n, 0);
		n++;
	}
	if (!n) do_menu(term, no_hist_menu, ses);
	else do_menu(term, mi, ses);
}


struct menu_item no_downloads_menu[] = {
	{TEXT(T_NO_DOWNLOADS), "", M_BAR, NULL, NULL, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

void
downloads_menu(struct terminal *term, void *ddd, struct session *ses)
{
	struct download *d;
	struct menu_item *mi = NULL;
	int n = 0;
	
	foreachback(d, downloads) {
		unsigned char *url;

		if (!mi) {
			mi = new_menu(3);
			if (!mi) return;
		}

		url = stracpy(d->url);
		if (strchr(url, POST_CHAR)) *strchr(url, POST_CHAR) = '\0';
		add_to_menu(&mi, url, "", "", MENU_FUNC display_download, d, 0);
		n++;
	}
	
	if (!n) {
		do_menu(term, no_downloads_menu, ses);
	} else {
		do_menu(term, mi, ses);
	}
}


void
menu_doc_info(struct terminal *term, void *ddd, struct session *ses)
{
	state_msg(ses);
}

void
menu_toggle(struct terminal *term, void *ddd, struct session *ses)
{
	toggle(ses, ses->screen, 0);
}

void
menu_shell(struct terminal *term, void *xxx, void *yyy)
{
	unsigned char *sh;
	if (!(sh = GETSHELL)) sh = DEFAULT_SHELL;
	exec_on_terminal(term, sh, "", 1);
}

void
menu_kill_background_connections(struct terminal *term, void *xxx, void *yyy)
{
	abort_background_connections();
}

struct menu_item file_menu11[] = {
	{TEXT(T_GOTO_URL), "g", TEXT(T_HK_GOTO_URL), MENU_FUNC menu_goto_url, (void *)0, 0, 0},
	{TEXT(T_GO_BACK), "<-", TEXT(T_HK_GO_BACK), MENU_FUNC menu_go_back, (void *)0, 0, 0},
	{TEXT(T_HISTORY), ">", TEXT(T_HK_HISTORY), MENU_FUNC history_menu, (void *)0, 1, 0},
	{TEXT(T_UNHISTORY), ">", TEXT(T_HK_UNHISTORY), MENU_FUNC unhistory_menu, (void *)0, 1, 0},
	{TEXT(T_RELOAD), "Ctrl-R", TEXT(T_HK_RELOAD), MENU_FUNC menu_reload, (void *)0, 0, 0},
};

struct menu_item file_menu12[] = {
#ifdef GLOBHIST
	{TEXT(T_GLOBAL_HISTORY), "h", TEXT(T_HK_GLOBAL_HISTORY), MENU_FUNC menu_history_manager, (void *)0, 0, 0},
#endif
#ifdef BOOKMARKS
	{TEXT(T_BOOKMARKS), "s", TEXT(T_HK_BOOKMARKS), MENU_FUNC menu_bookmark_manager, (void *)0, 0, 0},
	{TEXT(T_ADD_BOOKMARK), "a", TEXT(T_HK_ADD_BOOKMARK), MENU_FUNC launch_bm_add_doc_dialog, (void *)0, 0, 0},
#endif
};

struct menu_item file_menu21[] = {
	{"", "", M_BAR, NULL, NULL, 0, 0},
	{TEXT(T_SAVE_AS), "", TEXT(T_HK_SAVE_AS), MENU_FUNC save_as, (void *)0, 0, 0},
	{TEXT(T_SAVE_URL_AS), "", TEXT(T_HK_SAVE_URL_AS), MENU_FUNC menu_save_url_as, (void *)0, 0, 0},
	{TEXT(T_SAVE_FORMATTED_DOCUMENT), "", TEXT(T_HK_SAVE_FORMATTED_DOCUMENT), MENU_FUNC menu_save_formatted, (void *)0, 0, 0},
};

struct menu_item file_menu22[] = {
	{"", "", M_BAR, NULL, NULL, 0, 0},
	{TEXT(T_KILL_BACKGROUND_CONNECTIONS), "", TEXT(T_HK_KILL_BACKGROUND_CONNECTIONS), MENU_FUNC menu_kill_background_connections, (void *)0, 0, 0},
	{TEXT(T_FLUSH_ALL_CACHES), "", TEXT(T_HK_FLUSH_ALL_CACHES), MENU_FUNC flush_caches, (void *)0, 0, 0},
	{TEXT(T_RESOURCE_INFO), "", TEXT(T_HK_RESOURCE_INFO), MENU_FUNC res_inf, (void *)0, 0, 0},
#if 0
	/* This ought to be rewritten first, now it's too messy - try it with
	 * a bit full cache. */
	{TEXT(T_CACHE_INFO), "", TEXT(T_HK_CACHE_INFO), MENU_FUNC cache_inf, (void *)0, 0, 0},
#endif
#ifdef LEAK_DEBUG
	{TEXT(T_MEMORY_INFO), "", TEXT(T_HK_MEMORY_INFO), MENU_FUNC memory_inf, (void *)0, 0, 0},
#endif
	{"", "", M_BAR, NULL, NULL, 0, 0},
};

struct menu_item file_menu3[] = {
	{"", "", M_BAR, NULL, NULL, 0, 0},
	{TEXT(T_EXIT), "q", TEXT(T_HK_EXIT), MENU_FUNC exit_prog, (void *)0, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

void
do_file_menu(struct terminal *term, void *xxx, struct session *ses)
{
	int x;
	int o;
	struct menu_item *file_menu, *e, *f;
	if (!(file_menu = mem_alloc(sizeof(file_menu11) + sizeof(file_menu12) + sizeof(file_menu21) + sizeof(file_menu22) + sizeof(file_menu3) + 3 * sizeof(struct menu_item)))) return;
	e = file_menu;
	memcpy(e, file_menu11, sizeof(file_menu11));
	e += sizeof(file_menu11) / sizeof(struct menu_item);
	if (!get_opt_int_tree(cmdline_options, "anonymous")) {
		memcpy(e, file_menu12, sizeof(file_menu12));
		e += sizeof(file_menu12) / sizeof(struct menu_item);
	}
	if ((o = can_open_in_new(term))) {
		e->text = TEXT(T_NEW_WINDOW);
		e->rtext = o - 1 ? ">" : "";
		e->hotkey = TEXT(T_HK_NEW_WINDOW);
		e->func = MENU_FUNC open_in_new_window;
		e->data = send_open_new_xterm;
		e->in_m = o - 1;
		e->free_i = 0;
		e++;
	}
	if (!get_opt_int_tree(cmdline_options, "anonymous")) {
		memcpy(e, file_menu21, sizeof(file_menu21));
		e += sizeof(file_menu21) / sizeof(struct menu_item);
	}
	memcpy(e, file_menu22, sizeof(file_menu22));
	e += sizeof(file_menu22) / sizeof(struct menu_item);
	/*"", "", M_BAR, NULL, NULL, 0, 0,
	TEXT(T_OS_SHELL), "", TEXT(T_HK_OS_SHELL), MENU_FUNC menu_shell, NULL, 0, 0,*/
	x = 1;
	if (!get_opt_int_tree(cmdline_options, "anonymous")
	    && can_open_os_shell(term->environment)) {
		e->text = TEXT(T_OS_SHELL);
		e->rtext = "";
		e->hotkey = TEXT(T_HK_OS_SHELL);
		e->func = MENU_FUNC menu_shell;
		e->data = NULL;
		e->in_m = 0;
		e->free_i = 0;
		e++;
		x = 0;
	}
	if (can_resize_window(term->environment)) {
		e->text = TEXT(T_RESIZE_TERMINAL);
		e->rtext = "";
		e->hotkey = TEXT(T_HK_RESIZE_TERMINAL);
		e->func = MENU_FUNC dlg_resize_terminal;
		e->data = NULL;
		e->in_m = 0;
		e->free_i = 0;
		e++;
		x = 0;
	}
	memcpy(e, file_menu3 + x, sizeof(file_menu3) - x * sizeof(struct menu_item));
	e += sizeof(file_menu3) / sizeof(struct menu_item);
	for (f = file_menu; f < e; f++) f->free_i = 1;
	do_menu(term, file_menu, ses);
}

struct menu_item view_menu[] = {
	{TEXT(T_SEARCH), "/", TEXT(T_HK_SEARCH), MENU_FUNC menu_for_frame, (void *)search_dlg, 0, 0},
	{TEXT(T_SEARCH_BACK), "?", TEXT(T_HK_SEARCH_BACK), MENU_FUNC menu_for_frame, (void *)search_back_dlg, 0, 0},
	{TEXT(T_FIND_NEXT), "n", TEXT(T_HK_FIND_NEXT), MENU_FUNC menu_for_frame, (void *)find_next, 0, 0},
	{TEXT(T_FIND_PREVIOUS), "N", TEXT(T_HK_FIND_PREVIOUS), MENU_FUNC menu_for_frame, (void *)find_next_back, 0, 0},
	{"", "", M_BAR, NULL, NULL, 0, 0},
	{TEXT(T_TOGGLE_HTML_PLAIN), "\\", TEXT(T_HK_TOGGLE_HTML_PLAIN), MENU_FUNC menu_toggle, NULL, 0, 0},
	{TEXT(T_DOCUMENT_INFO), "=", TEXT(T_HK_DOCUMENT_INFO), MENU_FUNC menu_doc_info, NULL, 0, 0},
	{TEXT(T_FRAME_AT_FULL_SCREEN), "f", TEXT(T_HK_FRAME_AT_FULL_SCREEN), MENU_FUNC menu_for_frame, (void *)set_frame, 0, 0},
	{"", "", M_BAR, NULL, NULL, 0, 0},
	{TEXT(T_HTML_OPTIONS), "", TEXT(T_HK_HTML_OPTIONS), MENU_FUNC menu_html_options, (void *)0, 0, 0},
	{TEXT(T_SAVE_HTML_OPTIONS), "", TEXT(T_HK_SAVE_HTML_OPTIONS), MENU_FUNC menu_save_html_options, (void *)0, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

struct menu_item view_menu_anon[] = {
	{TEXT(T_SEARCH), "/", TEXT(T_HK_SEARCH), MENU_FUNC menu_for_frame, (void *)search_dlg, 0, 0},
	{TEXT(T_SEARCH_BACK), "?", TEXT(T_HK_SEARCH_BACK), MENU_FUNC menu_for_frame, (void *)search_back_dlg, 0, 0},
	{TEXT(T_FIND_NEXT), "n", TEXT(T_HK_FIND_NEXT), MENU_FUNC menu_for_frame, (void *)find_next, 0, 0},
	{TEXT(T_FIND_PREVIOUS), "N", TEXT(T_HK_FIND_PREVIOUS), MENU_FUNC menu_for_frame, (void *)find_next_back, 0, 0},
	{"", "", M_BAR, NULL, NULL, 0, 0},
	{TEXT(T_TOGGLE_HTML_PLAIN), "\\", TEXT(T_HK_TOGGLE_HTML_PLAIN), MENU_FUNC menu_toggle, NULL, 0, 0},
	{TEXT(T_DOCUMENT_INFO), "=", TEXT(T_HK_DOCUMENT_INFO), MENU_FUNC menu_doc_info, NULL, 0, 0},
	{TEXT(T_FRAME_AT_FULL_SCREEN), "f", TEXT(T_HK_FRAME_AT_FULL_SCREEN), MENU_FUNC menu_for_frame, (void *)set_frame, 0, 0},
	{"", "", M_BAR, NULL, NULL, 0, 0},
	{TEXT(T_HTML_OPTIONS), "", TEXT(T_HK_HTML_OPTIONS), MENU_FUNC menu_html_options, (void *)0, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

struct menu_item help_menu[] = {
	{TEXT(T_ABOUT), "", TEXT(T_HK_ABOUT), MENU_FUNC menu_about, (void *)0, 0, 0},
	{TEXT(T_KEYS), "", TEXT(T_HK_KEYS), MENU_FUNC menu_keys, (void *)0, 0, 0},
	{TEXT(T_MANUAL), "", TEXT(T_HK_MANUAL), MENU_FUNC menu_manual, (void *)0, 0, 0},
	{TEXT(T_COPYING), "", TEXT(T_HK_COPYING), MENU_FUNC menu_copying, (void *)0, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

#if 0
struct menu_item assoc_menu[] = {
	{TEXT(T_ADD), "", TEXT(T_HK_ADD), MENU_FUNC menu_add_ct, NULL, 0, 0},
	{TEXT(T_MODIFY), ">", TEXT(T_HK_MODIFY), MENU_FUNC menu_list_assoc, menu_add_ct, 1, 0},
	{TEXT(T_DELETE), ">", TEXT(T_HK_DELETE), MENU_FUNC menu_list_assoc, menu_del_ct, 1, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};
#endif

struct menu_item ext_menu[] = {
	{TEXT(T_ADD), "", TEXT(T_HK_ADD), MENU_FUNC menu_add_ext, NULL, 0, 0},
	{TEXT(T_MODIFY), ">", TEXT(T_HK_MODIFY), MENU_FUNC menu_list_ext, menu_add_ext, 1, 0},
	{TEXT(T_DELETE), ">", TEXT(T_HK_DELETE), MENU_FUNC menu_list_ext, menu_del_ext, 1, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

struct menu_item setup_menu[] = {
	{TEXT(T_LANGUAGE), ">", TEXT(T_HK_LANGUAGE), MENU_FUNC menu_language_list, NULL, 1, 0},
	{TEXT(T_CHARACTER_SET), ">", TEXT(T_HK_CHARACTER_SET), MENU_FUNC charset_list, (void *)1, 1, 0},
	{TEXT(T_TERMINAL_OPTIONS), "", TEXT(T_HK_TERMINAL_OPTIONS), MENU_FUNC terminal_options, NULL, 0, 0},
	{TEXT(T_NETWORK_OPTIONS), "", TEXT(T_HK_NETWORK_OPTIONS), MENU_FUNC net_options, NULL, 0, 0},
	{TEXT(T_CACHE), "", TEXT(T_HK_CACHE), MENU_FUNC cache_opt, NULL, 0, 0},
	{TEXT(T_MAIL_AND_TELNEL), "", TEXT(T_HK_MAIL_AND_TELNEL), MENU_FUNC net_programs, NULL, 0, 0},
/*	{TEXT(T_ASSOCIATIONS), ">", TEXT(T_HK_ASSOCIATIONS), MENU_FUNC do_menu, assoc_menu, 1, 0}, */
	{TEXT(T_FILE_EXTENSIONS), ">", TEXT(T_HK_FILE_EXTENSIONS), MENU_FUNC do_menu, ext_menu, 1, 0},
	{"", "", M_BAR, NULL, NULL, 0, 0},
	{TEXT(T_SAVE_OPTIONS), "", TEXT(T_HK_SAVE_OPTIONS), MENU_FUNC write_config, NULL, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

struct menu_item setup_menu_anon[] = {
	{TEXT(T_LANGUAGE), ">", TEXT(T_HK_LANGUAGE), MENU_FUNC menu_language_list, NULL, 1, 0},
	{TEXT(T_CHARACTER_SET), ">", TEXT(T_HK_CHARACTER_SET), MENU_FUNC charset_list, (void *)1, 1, 0},
	{TEXT(T_TERMINAL_OPTIONS), "", TEXT(T_HK_TERMINAL_OPTIONS), MENU_FUNC terminal_options, NULL, 0, 0},
	{TEXT(T_NETWORK_OPTIONS), "", TEXT(T_HK_NETWORK_OPTIONS), MENU_FUNC net_options, NULL, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

void
do_view_menu(struct terminal *term, void *xxx, struct session *ses)
{
	if (!get_opt_int_tree(cmdline_options, "anonymous")) do_menu(term, view_menu, ses);
	else do_menu(term, view_menu_anon, ses);
}

void
do_setup_menu(struct terminal *term, void *xxx, struct session *ses)
{
	if (!get_opt_int_tree(cmdline_options, "anonymous")) do_menu(term, setup_menu, ses);
	else do_menu(term, setup_menu_anon, ses);
}

struct menu_item main_menu[] = {
	{TEXT(T_FILE), "", TEXT(T_HK_FILE), MENU_FUNC do_file_menu, NULL, 1, 1},
	{TEXT(T_VIEW), "", TEXT(T_HK_VIEW), MENU_FUNC do_view_menu, NULL, 1, 1},
	{TEXT(T_LINK), "", TEXT(T_HK_LINK), MENU_FUNC link_menu, NULL, 1, 1},
	{TEXT(T_DOWNLOADS), "", TEXT(T_HK_DOWNLOADS), MENU_FUNC downloads_menu, NULL, 1, 1},
	{TEXT(T_SETUP), "", TEXT(T_HK_SETUP), MENU_FUNC do_setup_menu, NULL, 1, 1},
	{TEXT(T_HELP), "", TEXT(T_HK_HELP), MENU_FUNC do_menu, help_menu, 1, 1},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

void
activate_bfu_technology(struct session *ses, int item)
{
	struct terminal *term = ses->term;
	do_mainmenu(term, main_menu, ses, item);
}

struct input_history goto_url_history = { 0, {&goto_url_history.items, &goto_url_history.items} };

void
goto_url_with_hook(struct session *ses, unsigned char *url)
{
#ifdef HAVE_SCRIPTING
	script_hook_goto_url(ses, url);
#else
	goto_url(ses, url);
#endif
}

void
dialog_goto_url(struct session *ses, char *url)
{
	input_field(ses->term, NULL, TEXT(T_GOTO_URL), TEXT(T_ENTER_URL), TEXT(T_OK), TEXT(T_CANCEL), ses, &goto_url_history, MAX_STR_LEN, url, 0, 0, NULL, (void (*)(void *, unsigned char *)) goto_url_with_hook, NULL);
}

void
dialog_save_url(struct session *ses)
{
	input_field(ses->term, NULL, TEXT(T_SAVE_URL), TEXT(T_ENTER_URL), TEXT(T_OK), TEXT(T_CANCEL), ses, &goto_url_history, MAX_STR_LEN, "", 0, 0, NULL, (void (*)(void *, unsigned char *)) save_url, NULL);
}

struct input_history file_history = { 0, {&file_history.items, &file_history.items} };

void
query_file(struct session *ses, unsigned char *url, void (*std)(struct session *, unsigned char *), void (*cancel)(struct session *))
{
	unsigned char *file, *def;
	int dfl = 0;
	int l;
	get_filename_from_url(url, &file, &l);
	def = init_str();
	add_to_str(&def, &dfl, get_opt_str("document.download.directory"));
	if (*def && !dir_sep(def[strlen(def) - 1])) add_chr_to_str(&def, &dfl, '/');
	add_bytes_to_str(&def, &dfl, file, l);
	input_field(ses->term, NULL, TEXT(T_DOWNLOAD), TEXT(T_SAVE_TO_FILE), TEXT(T_OK), TEXT(T_CANCEL), ses, &file_history, MAX_STR_LEN, def, 0, 0, NULL, (void (*)(void *, unsigned char *))std, (void (*)(void *))cancel);
	mem_free(def);
}

struct input_history search_history = { 0, {&search_history.items, &search_history.items} };

void
search_back_dlg(struct session *ses, struct f_data_c *f, int a)
{
	input_field(ses->term, NULL, TEXT(T_SEARCH_BACK), TEXT(T_SEARCH_FOR_TEXT), TEXT(T_OK), TEXT(T_CANCEL), ses, &search_history, MAX_STR_LEN, "", 0, 0, NULL, (void (*)(void *, unsigned char *)) search_for_back, NULL);
}

void
search_dlg(struct session *ses, struct f_data_c *f, int a)
{
	input_field(ses->term, NULL, TEXT(T_SEARCH), TEXT(T_SEARCH_FOR_TEXT), TEXT(T_OK), TEXT(T_CANCEL), ses, &search_history, MAX_STR_LEN, "", 0, 0, NULL, (void (*)(void *, unsigned char *)) search_for, NULL);
}

void
free_history_lists()
{
	free_list(goto_url_history.items);
	free_list(file_history.items);
	free_list(search_history.items);
#ifdef HAVE_LUA
	free_lua_console_history();
#endif
}

