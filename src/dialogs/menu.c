/* Menu system */
/* $Id: menu.c,v 1.149 2003/10/18 23:14:21 pasky Exp $ */

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
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/options.h"
#include "config/urlhist.h"
#include "dialogs/document.h"
#include "dialogs/info.h"
#include "dialogs/menu.h"
#include "dialogs/mime.h"
#include "dialogs/options.h"
#include "globhist/dialogs.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/download.h"
#include "sched/event.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/session.h"
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
	close_tab(term);
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
	reload(ses, -1);
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

static void
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
go_backwards(struct terminal *term, void *psteps, struct session *ses)
{
	int steps = (int) psteps;

	abort_loading(ses, 0);

	/* Move all intermediate items to unhistory... */

	while (steps-- > 0) {
		struct location *loc = ses->history.next;

		if (!have_location(ses)) return;

		/* First item in history/unhistory is something special and
		 * precious... like... like... the current location? */

		loc = loc->next;
		if ((void *) loc == &ses->history) return;

		go_back(ses);
	}
}

static void
go_unbackwards(struct terminal *term, void *psteps, struct session *ses)
{
	int steps = (int) psteps + 1;

	abort_loading(ses, 0);

	/* Move all intermediate items to history... */

	while (steps-- > 0) {
	    	struct location *loc = ses->unhistory.next;

		if ((void *) loc == &ses->unhistory) return;

		go_unback(ses);
	}
}

static struct menu_item no_hist_menu[] = {
	INIT_MENU_ITEM(N_("No history"), M_BAR, NULL, NULL, FREE_NOTHING, 0),
	NULL_MENU_ITEM
};

static void
history_menu(struct terminal *term, void *ddd, struct session *ses)
{
	struct location *loc;
	struct menu_item *mi = NULL;
	int n = 0;

	foreach (loc, ses->history) {
		unsigned char *url;

		if (!n) {
			n++;
			continue;
		}

		if (!mi) {
			mi = new_menu(FREE_LIST | FREE_TEXT);
			if (!mi) return;
		}

		url = memacpy(loc->vs.url, loc->vs.url_len);
		if (url) {
			unsigned char *pc = strchr(url, POST_CHAR);
			if (pc) *pc = '\0';

			add_to_menu(&mi, url, "", (menu_func) go_backwards,
			    	    (void *) n, 0, 1);
		}
		n++;
	}

	if (n <= 1)
		do_menu(term, no_hist_menu, ses, 0);
	else
		do_menu(term, mi, ses, 0);
}

static void
unhistory_menu(struct terminal *term, void *ddd, struct session *ses)
{
	struct location *loc;
	struct menu_item *mi = NULL;
	int n = 0;

	foreach (loc, ses->unhistory) {
		unsigned char *url;

		if (!mi) {
			mi = new_menu(FREE_LIST | FREE_TEXT);
			if (!mi) return;
		}

		url = memacpy(loc->vs.url, loc->vs.url_len);
		if (url) {
			unsigned char *pc = strchr(url, POST_CHAR);
			if (pc) *pc = '\0';

			add_to_menu(&mi, url, "", (menu_func) go_unbackwards,
			    	    (void *) n, 0, 1);
		}
		n++;
	}

	if (!n)
		do_menu(term, no_hist_menu, ses, 0);
	else
		do_menu(term, mi, ses, 0);
}


static struct menu_item no_downloads_menu[] = {
	INIT_MENU_ITEM(N_("No downloads"), M_BAR, NULL, NULL, FREE_NOTHING, 0),
	NULL_MENU_ITEM
};

static void
downloads_menu(struct terminal *term, void *ddd, struct session *ses)
{
	struct file_download *d;
	struct menu_item *mi = NULL;
	int n = 0;

	foreachback (d, downloads) {
		unsigned char *url;

		if (!mi) {
			mi = new_menu(FREE_LIST | FREE_TEXT);
			if (!mi) return;
		}

		url = stracpy(d->url);
		if (url) {
			unsigned char *pc = strchr(url, POST_CHAR);
			if (pc) *pc = '\0';

			add_to_menu(&mi, url, "", (menu_func) display_download,
			    	    d, 0, 1);
			n++;
		}
	}

	if (!n)
		do_menu(term, no_downloads_menu, ses, 0);
	else
		do_menu(term, mi, ses, 0);
}


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
menu_toggle(struct terminal *term, void *ddd, struct session *ses)
{
	toggle(ses, ses->doc_view, 0);
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
	INIT_MENU_ITEM(N_("Open new ~tab"), "t", open_in_new_tab, (void *) 0, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Open new tab in ~background"), "T", open_in_new_tab_in_background,
								(void *) 0, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~Go to URL"), "g", menu_goto_url, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Go ~back"), "<-", menu_go_back, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~Reload"), "Ctrl-R", menu_reload, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~History"), M_SUBMENU, history_menu, NULL, FREE_NOTHING, 1),
	INIT_MENU_ITEM(N_("Unh~istory"), M_SUBMENU, unhistory_menu, NULL, FREE_NOTHING, 1),
};

static struct menu_item file_menu12[] = {
#ifdef GLOBHIST
	INIT_MENU_ITEM(N_("Global histor~y"), "h", menu_history_manager, NULL, FREE_NOTHING, 0),
#endif
#ifdef BOOKMARKS
	INIT_MENU_ITEM(N_("Bookmark~s"), "s", menu_bookmark_manager, NULL, FREE_NOTHING, 0),
#endif
};

static struct menu_item file_menu21[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("Sa~ve as"), "", save_as, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Save ~URL as"), "", menu_save_url_as, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Save formatted ~document"), "", menu_save_formatted, NULL, FREE_NOTHING, 0),
};

static struct menu_item file_menu22[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("~Kill background connections"), "", menu_kill_background_connections, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~Flush all caches"), "", flush_caches, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Resource in~fo"), "", res_inf, NULL, FREE_NOTHING, 0),
#ifdef DEBUG
	INIT_MENU_ITEM(N_("~Cache info"), "", cache_inf, NULL, FREE_NOTHING, 0),
#endif
#ifdef LEAK_DEBUG
	INIT_MENU_ITEM(N_("~Memory info"), "", memory_inf, NULL, FREE_NOTHING, 0),
#endif
	BAR_MENU_ITEM,
};

static struct menu_item file_menu3[] = {
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("E~xit"), "q", exit_prog, NULL, FREE_NOTHING, 0),
	NULL_MENU_ITEM,
};

static void
do_file_menu(struct terminal *term, void *xxx, struct session *ses)
{
	struct menu_item *file_menu, *e, *f;
	int anonymous = get_opt_int_tree(cmdline_options, "anonymous");
	int x, o;

	file_menu = mem_alloc(sizeof(file_menu11) + sizeof(file_menu12)
			      + sizeof(file_menu21) + sizeof(file_menu22)
			      + sizeof(file_menu3)
			      + 3 * sizeof(struct menu_item));
	if (!file_menu) return;

	e = file_menu;
	o = can_open_in_new(term);
	if (o) {
		SET_MENU_ITEM(e, N_("~New window"), o - 1 ? M_SUBMENU : (unsigned char *) "",
			      (menu_func) open_in_new_window, send_open_new_window,
			      FREE_NOTHING, !!(o - 1), 0, 0, HKS_SHOW);
		e++;
	}

	memcpy(e, file_menu11, sizeof(file_menu11));
	e += sizeof(file_menu11) / sizeof(struct menu_item);

	if (!anonymous) {
		memcpy(e, file_menu12, sizeof(file_menu12));
		e += sizeof(file_menu12) / sizeof(struct menu_item);

		memcpy(e, file_menu21, sizeof(file_menu21));
		e += sizeof(file_menu21) / sizeof(struct menu_item);
	}

	memcpy(e, file_menu22, sizeof(file_menu22));
	e += sizeof(file_menu22) / sizeof(struct menu_item);

	x = 1;
	if (!anonymous && can_open_os_shell(term->environment)) {
		SET_MENU_ITEM(e, N_("~OS shell"), "", menu_shell,
			      NULL, FREE_NOTHING, 0, 0, 0, HKS_SHOW);
		e++;
		x = 0;
	}

	if (can_resize_window(term->environment)) {
		SET_MENU_ITEM(e, N_("Resize t~erminal"), "", dlg_resize_terminal,
			      NULL, FREE_NOTHING, 0, 0, 0, HKS_SHOW);
		e++;
		x = 0;
	}

	memcpy(e, file_menu3 + x, sizeof(file_menu3) - x * sizeof(struct menu_item));
	e += sizeof(file_menu3) / sizeof(struct menu_item);

	for (f = file_menu; f < e; f++)
		f->item_free = FREE_LIST;

	do_menu(term, file_menu, ses, 1);
}

static struct menu_item view_menu[] = {
	INIT_MENU_ITEM(N_("~Search"), "/", menu_for_frame, (void *)search_dlg, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Search ~backward"), "?", menu_for_frame, (void *)search_back_dlg, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Find ~next"), "n", menu_for_frame, (void *)find_next, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Find ~previous"), "N", menu_for_frame, (void *)find_next_back, FREE_NOTHING, 0),
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("Toggle ~html/plain"), "\\", menu_toggle, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Document ~info"), "=", menu_doc_info, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("H~eader info"), "|", menu_header_info, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Frame at ~full-screen"), "f", menu_for_frame, (void *)set_frame, FREE_NOTHING, 0),
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("Nex~t tab"), ">", menu_next_tab, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("Pre~v tab"), "<", menu_prev_tab, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~Close tab"), "c", menu_close_tab, NULL, FREE_NOTHING, 0),
	NULL_MENU_ITEM
};


static struct menu_item help_menu[] = {
	INIT_MENU_ITEM(N_("~ELinks homepage"), "", menu_url_shortcut, ELINKS_HOMEPAGE, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~Documentation"), "", menu_url_shortcut, ELINKS_DOC_URL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~Keys"), "", menu_keys, NULL, FREE_NOTHING, 0),
	BAR_MENU_ITEM,
#ifdef DEBUG
	INIT_MENU_ITEM(N_("~Bugs information"), "", menu_url_shortcut, ELINKS_BUGS_URL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~ELinks CvsWeb"), "", menu_url_shortcut, ELINKS_CVSWEB_URL, FREE_NOTHING, 0),
	BAR_MENU_ITEM,
#endif
	INIT_MENU_ITEM(N_("~Copying"), "", menu_copying, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~About"), "", menu_about, NULL, FREE_NOTHING, 0),
	NULL_MENU_ITEM
};


static struct menu_item ext_menu[] = {
	INIT_MENU_ITEM(N_("~Add"), "", menu_add_ext, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~Modify"), M_SUBMENU, menu_list_ext, menu_add_ext, FREE_NOTHING, 1),
	INIT_MENU_ITEM(N_("~Delete"), M_SUBMENU, menu_list_ext, menu_del_ext, FREE_NOTHING, 1),
	NULL_MENU_ITEM
};

static inline void
do_ext_menu(struct terminal *term, void *xxx, struct session *ses)
{
	do_menu(term, ext_menu, ses, 1);
}

static struct menu_item setup_menu[] = {
#ifdef ENABLE_NLS
	INIT_MENU_ITEM(N_("~Language"), M_SUBMENU, menu_language_list, NULL, FREE_NOTHING, 1),
#endif
	INIT_MENU_ITEM(N_("C~haracter set"), M_SUBMENU, charset_list, NULL, FREE_NOTHING, 1),
	INIT_MENU_ITEM(N_("~Terminal options"), "", terminal_options, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("File ~extensions"), M_SUBMENU, do_ext_menu, NULL, FREE_NOTHING, 1),
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("~Options manager"), "o", menu_options_manager, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~Keybinding manager"), "k", menu_keybinding_manager, NULL, FREE_NOTHING, 0),
	INIT_MENU_ITEM(N_("~Save options"), "", write_config, NULL, FREE_NOTHING, 0),
	NULL_MENU_ITEM
};

static struct menu_item setup_menu_anon[] = {
	INIT_MENU_ITEM(N_("~Language"), M_SUBMENU, menu_language_list, NULL, FREE_NOTHING, 1),
	INIT_MENU_ITEM(N_("C~haracter set"), M_SUBMENU, charset_list, NULL, FREE_NOTHING, 1),
	INIT_MENU_ITEM(N_("~Terminal options"), "", terminal_options, NULL, FREE_NOTHING, 0),
	NULL_MENU_ITEM
};

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
	INIT_MENU_ITEM(N_("~File"), "", do_file_menu, NULL, FREE_LIST, 1),
	INIT_MENU_ITEM(N_("~View"), "", do_view_menu, NULL, FREE_LIST, 1),
	INIT_MENU_ITEM(N_("~Link"), "", link_menu, NULL, FREE_LIST, 1),
	INIT_MENU_ITEM(N_("~Downloads"), "", downloads_menu, NULL, FREE_LIST, 1),
	INIT_MENU_ITEM(N_("~Setup"), "", do_setup_menu, NULL, FREE_LIST, 1),
	INIT_MENU_ITEM(N_("~Help"), "", do_help_menu, NULL, FREE_LIST, 1),
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

static struct input_history file_history = { 0, {D_LIST_HEAD(file_history.items)} };

void
query_file(struct session *ses, unsigned char *url,
	   void (*std)(struct session *, unsigned char *),
	   void (*cancel)(struct session *), int interactive)
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
			    N_("OK"),  N_("Cancel"), ses, &file_history,
			    MAX_STR_LEN, def.source, 0, 0, NULL,
			    (void (*)(void *, unsigned char *)) std,
			    (void (*)(void *)) cancel);
	} else {
		std(ses, def.source);
	}

	done_string(&def);
}

void
free_history_lists(void)
{
	free_list(goto_url_history.items);
	free_list(file_history.items);
	free_list(search_history.items);
#ifdef HAVE_SCRIPTING
	trigger_event_name("free-history");
#endif
}
