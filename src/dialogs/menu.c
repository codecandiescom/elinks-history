/* Menu system */
/* $Id: menu.c,v 1.118 2003/07/05 10:24:40 zas Exp $ */

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
#include "terminal/kbd.h"
#include "terminal/tab.h"
#include "lowlevel/select.h"
#include "terminal/terminal.h"
#include "protocol/url.h"
#include "sched/download.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "scripting/lua/core.h"
#include "scripting/lua/hooks.h"
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
	       void (*f)(struct session *, struct f_data_c *, int),
	       struct session *ses)
{
	if (!have_location(ses)) return;
	do_for_frame(ses, f, 0);
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

#if 0
	if (ses->tq_goto_position)
		--steps;
	if (ses->search_word)
		mem_free(ses->search_word), ses->search_word = NULL;
#endif

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
	{N_("No history"), M_BAR, NULL, NULL, 0, 0},
	{NULL, NULL, NULL, NULL, 0, 0}
};

static void
history_menu(struct terminal *term, void *ddd, struct session *ses)
{
	struct location *l;
	struct menu_item *mi = NULL;
	int n = 0;

	foreach (l, ses->history) {
		unsigned char *url;

		if (!n) {
			n++;
			continue;
		}

		if (!mi) {
			mi = new_menu(FREE_LIST | FREE_TEXT);
			if (!mi) return;
		}

		url = stracpy(l->vs.url);
		if (url) {
			unsigned char *pc = strchr(url, POST_CHAR);
			if (pc) *pc = '\0';

			add_to_menu(&mi, url, "", MENU_FUNC go_backwards,
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
	struct location *l;
	struct menu_item *mi = NULL;
	int n = 0;

	foreach (l, ses->unhistory) {
		unsigned char *url;

		if (!mi) {
			mi = new_menu(FREE_LIST | FREE_TEXT);
			if (!mi) return;
		}

		url = stracpy(l->vs.url);
		if (url) {
			unsigned char *pc = strchr(url, POST_CHAR);
			if (pc) *pc = '\0';

			add_to_menu(&mi, url, "", MENU_FUNC go_unbackwards,
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
	{N_("No downloads"), M_BAR, NULL, NULL, 0, 0},
	{NULL, NULL, NULL, NULL, 0, 0}
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

			add_to_menu(&mi, url, "", MENU_FUNC display_download,
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
	toggle(ses, ses->screen, 0);
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
	{N_("~Go to URL"), "g", MENU_FUNC menu_goto_url, (void *)0, 0, 0},
	{N_("Go ~back"), "<-", MENU_FUNC menu_go_back, (void *)0, 0, 0},
	{N_("~Reload"), "Ctrl-R", MENU_FUNC menu_reload, (void *)0, 0, 0},
	{N_("~History"), M_SUBMENU, MENU_FUNC history_menu, (void *)0, 0, 1},
	{N_("Unhis~tory"), M_SUBMENU, MENU_FUNC unhistory_menu, (void *)0, 0, 1},
};

static struct menu_item file_menu12[] = {
#ifdef GLOBHIST
	{N_("Global histor~y"), "h", MENU_FUNC menu_history_manager, (void *)0, 0, 0},
#endif
#ifdef BOOKMARKS
	{N_("Bookmark~s"), "s", MENU_FUNC menu_bookmark_manager, (void *)0, 0, 0},
#endif
};

static struct menu_item file_menu21[] = {
	{"", M_BAR, NULL, NULL, 0, 0},
	{N_("Sa~ve as"), "", MENU_FUNC save_as, (void *)0, 0, 0},
	{N_("Save ~URL as"), "", MENU_FUNC menu_save_url_as, (void *)0, 0, 0},
	{N_("Save formatted ~document"), "", MENU_FUNC menu_save_formatted, (void *)0, 0, 0},
};

static struct menu_item file_menu22[] = {
	{"", M_BAR, NULL, NULL, 0, 0},
	{N_("~Kill background connections"), "", MENU_FUNC menu_kill_background_connections, (void *)0, 0, 0},
	{N_("~Flush all caches"), "", MENU_FUNC flush_caches, (void *)0, 0, 0},
	{N_("Resource ~info"), "", MENU_FUNC res_inf, (void *)0, 0, 0},
#if 1 /* Always visible ? --Zas */
	{N_("~Cache info"), "", MENU_FUNC cache_inf, (void *)0, 0, 0},
#endif
#ifdef LEAK_DEBUG
	{N_("~Memory info"), "", MENU_FUNC memory_inf, (void *)0, 0, 0},
#endif
	{"", M_BAR, NULL, NULL, 0, 0},
};

static struct menu_item file_menu3[] = {
	{"", M_BAR, NULL, NULL, 0, 0},
	{N_("E~xit"), "q", MENU_FUNC exit_prog, (void *)0, 0, 0},
	{NULL, NULL, NULL, NULL, 0, 0}
};

static void
do_file_menu(struct terminal *term, void *xxx, struct session *ses)
{
	int x;
	int o;
	struct menu_item *file_menu, *e, *f;
	int anonymous = get_opt_int_tree(&cmdline_options, "anonymous");

	file_menu = mem_alloc(sizeof(file_menu11) + sizeof(file_menu12)
			      + sizeof(file_menu21) + sizeof(file_menu22)
			      + sizeof(file_menu3)
			      + 3 * sizeof(struct menu_item));

	if (!file_menu) return;
	e = file_menu;

	o = can_open_in_new(term);
	if (o) {
		e->text = N_("~New window");
		e->rtext = o - 1 ? M_SUBMENU : (unsigned char *) "";
		e->func = MENU_FUNC open_in_new_window;
		e->data = send_open_new_xterm;
		e->item_free = FREE_NOTHING;
		e->submenu = !!(o - 1);
		e->no_intl = 0;
		e->hotkey_pos = 0;
		e->ignore_hotkey = 0;
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
	/*"", M_BAR, NULL, NULL, 0, 0,
	N_("~OS shell"), "", MENU_FUNC menu_shell, NULL, 0, 0,*/

	x = 1;
	if (!anonymous && can_open_os_shell(term->environment)) {
		e->text = N_("~OS shell");
		e->rtext = "";
		e->func = MENU_FUNC menu_shell;
		e->data = NULL;
		e->item_free = FREE_NOTHING;
		e->submenu = 0;
		e->no_intl = 0;
		e->hotkey_pos = 0;
		e->ignore_hotkey = 0;
		e++;
		x = 0;
	}

	if (can_resize_window(term->environment)) {
		e->text = N_("Resize ~terminal");
		e->rtext = "";
		e->func = MENU_FUNC dlg_resize_terminal;
		e->data = NULL;
		e->item_free = FREE_NOTHING;
		e->submenu = 0;
		e->no_intl = 0;
		e->hotkey_pos = 0;
		e->ignore_hotkey = 0;
		e++;
		x = 0;
	}

	memcpy(e, file_menu3 + x, sizeof(file_menu3) - x * sizeof(struct menu_item));
	e += sizeof(file_menu3) / sizeof(struct menu_item);

	for (f = file_menu; f < e; f++) f->item_free = FREE_LIST;

	do_menu(term, file_menu, ses, 1);
}

static struct menu_item view_menu[] = {
	{N_("~Search"), "/", MENU_FUNC menu_for_frame, (void *)search_dlg, 0, 0},
	{N_("Search ~backward"), "?", MENU_FUNC menu_for_frame, (void *)search_back_dlg, 0, 0},
	{N_("Find ~next"), "n", MENU_FUNC menu_for_frame, (void *)find_next, 0, 0},
	{N_("Find ~previous"), "N", MENU_FUNC menu_for_frame, (void *)find_next_back, 0, 0},
	{"", M_BAR, NULL, NULL, 0, 0},
	{N_("Toggle ~html/plain"), "\\", MENU_FUNC menu_toggle, NULL, 0, 0},
	{N_("Document ~info"), "=", MENU_FUNC menu_doc_info, NULL, 0, 0},
	{N_("H~eader info"), "|", MENU_FUNC menu_header_info, NULL, 0, 0},
	{N_("Frame at ~full-screen"), "f", MENU_FUNC menu_for_frame, (void *)set_frame, 0, 0},
	{"", M_BAR, NULL, NULL, 0, 0},
	{N_("Nex~t tab"), ">", MENU_FUNC menu_next_tab, NULL, 0, 0},
	{N_("Pre~v tab"), "<", MENU_FUNC menu_prev_tab, NULL, 0, 0},
	{N_("~Close tab"), "c", MENU_FUNC menu_close_tab, NULL, 0, 0},
	{NULL, NULL, NULL, NULL, 0, 0}
};

static struct menu_item view_menu_anon[] = {
	{N_("~Search"), "/", MENU_FUNC menu_for_frame, (void *)search_dlg, 0, 0},
	{N_("Search ~backward"), "?", MENU_FUNC menu_for_frame, (void *)search_back_dlg, 0, 0},
	{N_("Find ~next"), "n", MENU_FUNC menu_for_frame, (void *)find_next, 0, 0},
	{N_("Find ~previous"), "N", MENU_FUNC menu_for_frame, (void *)find_next_back, 0, 0},
	{"", M_BAR, NULL, NULL, 0, 0},
	{N_("Toggle ~html/plain"), "\\", MENU_FUNC menu_toggle, NULL, 0, 0},
	{N_("Document ~info"), "=", MENU_FUNC menu_doc_info, NULL, 0, 0},
	{N_("Frame at ~full-screen"), "f", MENU_FUNC menu_for_frame, (void *)set_frame, 0, 0},
	{NULL, NULL, NULL, NULL, 0, 0}
};

static struct menu_item help_menu[] = {
	{N_("~ELinks homepage"), "", MENU_FUNC menu_url_shortcut, (void *)ELINKS_HOMEPAGE, 0, 0},
	{N_("~Documentation"), "", MENU_FUNC menu_url_shortcut, (void *)ELINKS_DOC_URL, 0, 0},
	{N_("~Keys"), "", MENU_FUNC menu_keys, (void *)0, 0, 0},
	{"", M_BAR, NULL, NULL, 0, 0},
#ifdef DEBUG
	{N_("~Bugs information"), "", MENU_FUNC menu_url_shortcut, (void *)ELINKS_BUGS_URL, 0, 0},
	{N_("~ELinks CvsWeb"), "", MENU_FUNC menu_url_shortcut, (void *)ELINKS_CVSWEB_URL, 0, 0},
	{"", M_BAR, NULL, NULL, 0, 0},
#endif
	{N_("~Copying"), "", MENU_FUNC menu_copying, (void *)0, 0, 0},
	{N_("~About"), "", MENU_FUNC menu_about, (void *)0, 0, 0},
	{NULL, NULL, NULL, NULL, 0, 0}
};

#if 0
static struct menu_item assoc_menu[] = {
	{N_("~Add"), "", MENU_FUNC menu_add_ct, NULL, 0, 0},
	{N_("~Modify"), M_SUBMENU, MENU_FUNC menu_list_assoc, menu_add_ct, 1, 0},
	{N_("~Delete"), M_SUBMENU, MENU_FUNC menu_list_assoc, menu_del_ct, 1, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};
#endif

static struct menu_item ext_menu[] = {
	{N_("~Add"), "", MENU_FUNC menu_add_ext, NULL, 0, 0},
	{N_("~Modify"), M_SUBMENU, MENU_FUNC menu_list_ext, menu_add_ext, 0, 1},
	{N_("~Delete"), M_SUBMENU, MENU_FUNC menu_list_ext, menu_del_ext, 0, 1},
	{NULL, NULL, NULL, NULL, 0, 0}
};

static inline void
do_ext_menu(struct terminal *term, void *xxx, struct session *ses)
{
	do_menu(term, ext_menu, ses, 1);
}

static struct menu_item setup_menu[] = {
#ifdef ENABLE_NLS
	{N_("~Language"), M_SUBMENU, MENU_FUNC menu_language_list, NULL, 0, 1},
#endif
	{N_("C~haracter set"), M_SUBMENU, MENU_FUNC charset_list, (void *)1, 0, 1},
	{N_("~Terminal options"), "", MENU_FUNC terminal_options, NULL, 0, 0},
/*	{N_("~Associations"), M_SUBMENU, MENU_FUNC do_menu, assoc_menu, 0, 1}, */
	{N_("File ~extensions"), M_SUBMENU, MENU_FUNC do_ext_menu, NULL, 0, 1},
	{"", M_BAR, NULL, NULL, 0, 0},
	{N_("~Options manager"), "o", MENU_FUNC menu_options_manager, NULL, 0, 0},
	{N_("~Keybinding manager"), "k", MENU_FUNC menu_keybinding_manager, NULL, 0, 0},
	{N_("~Save options"), "", MENU_FUNC write_config, NULL, 0, 0},
	{NULL, NULL, NULL, NULL, 0, 0}
};

static struct menu_item setup_menu_anon[] = {
	{N_("~Language"), M_SUBMENU, MENU_FUNC menu_language_list, NULL, 0, 1},
	{N_("C~haracter set"), M_SUBMENU, MENU_FUNC charset_list, (void *)1, 0, 1},
	{N_("~Terminal options"), "", MENU_FUNC terminal_options, NULL, 0, 0},
	{NULL, NULL, NULL, NULL, 0, 0}
};

static void
do_view_menu(struct terminal *term, void *xxx, struct session *ses)
{
	if (!get_opt_int_tree(&cmdline_options, "anonymous"))
		do_menu(term, view_menu, ses, 1);
	else
		do_menu(term, view_menu_anon, ses, 1);
}

static void
do_setup_menu(struct terminal *term, void *xxx, struct session *ses)
{
	if (!get_opt_int_tree(&cmdline_options, "anonymous"))
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
	{N_("~File"), "", MENU_FUNC do_file_menu, NULL, 1, 1},
	{N_("~View"), "", MENU_FUNC do_view_menu, NULL, 1, 1},
	{N_("~Link"), "", MENU_FUNC link_menu, NULL, 1, 1},
	{N_("~Downloads"), "", MENU_FUNC downloads_menu, NULL, 1, 1},
	{N_("~Setup"), "", MENU_FUNC do_setup_menu, NULL, 1, 1},
	{N_("~Help"), "", MENU_FUNC do_help_menu, NULL, 1, 1},
	{NULL, NULL, NULL, NULL, 0, 0}
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
	unsigned char *file;
	unsigned char *def = init_str();
	int dfl = 0;
	int l;

	if (!def) return;

	get_filename_from_url(url, &file, &l);

	add_to_str(&def, &dfl, get_opt_str("document.download.directory"));
	if (*def && !dir_sep(def[strlen(def) - 1])) add_chr_to_str(&def, &dfl, '/');
	add_bytes_to_str(&def, &dfl, file, l);

	if (interactive) {
		input_field(ses->tab->term, NULL, 1,
			    N_("Download"), N_("Save to file"),
			    N_("OK"),  N_("Cancel"), ses, &file_history,
			    MAX_STR_LEN, def, 0, 0, NULL,
			    (void (*)(void *, unsigned char *)) std,
			    (void (*)(void *)) cancel);
	} else {
		std(ses, def);
	}

	mem_free(def);
}

static struct input_history search_history = { 0, {D_LIST_HEAD(search_history.items)} };

void
search_back_dlg(struct session *ses, struct f_data_c *f, int a)
{
	input_field(ses->tab->term, NULL, 1,
		    N_("Search backward"), N_("Search for text"),
		    N_("OK"), N_("Cancel"), ses, &search_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) search_for_back,
		    NULL);
}

void
search_dlg(struct session *ses, struct f_data_c *f, int a)
{
	input_field(ses->tab->term, NULL, 1,
		    N_("Search"), N_("Search for text"),
		    N_("OK"), N_("Cancel"), ses, &search_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) search_for,
		    NULL);
}

void
free_history_lists(void)
{
	free_list(goto_url_history.items);
	free_list(file_history.items);
	free_list(search_history.items);
#ifdef HAVE_LUA
	free_lua_console_history();
#endif
}
