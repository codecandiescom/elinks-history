/* Input history for input fields. */
/* $Id: inphist.c,v 1.37 2003/10/26 12:26:04 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/menu.h"
#include "config/options.h"
#include "config/urlhist.h"
#include "lowlevel/home.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/secsave.h"


static void
tab_compl_n(struct terminal *term, unsigned char *item, int len,
	    struct window *win)
{
	struct term_event ev = INIT_TERM_EVENT(EV_REDRAW, term->x, term->y, 0);
	struct dialog_data *dlg_data = (struct dialog_data *) win->data;
	struct widget_data *di = selected_widget(dlg_data);

	int_upper_bound(&len, di->item->dlen - 1);
	memcpy(di->cdata, item, len);
	di->cdata[len] = 0;
	di->cpos = len;
	di->vpos = 0;
	dialog_func(win, &ev, 0);
}

static inline void
tab_compl(struct terminal *term, unsigned char *item, struct window *win)
{
	tab_compl_n(term, item, strlen(item), win);
}

/* Allocate "+ 2" since the last entry is later memset() in do_tab_compl(). */
#define realloc_menu_items(menu, size) \
	mem_align_alloc(menu, size, (size) + 2, sizeof(struct menu_item), 0xFF)

/* Complete to last unambiguous character, and display menu for all possible
 * further completions. */
void
do_tab_compl(struct terminal *term, struct list_head *history,
	     struct window *win)
{
	struct dialog_data *dlg_data = (struct dialog_data *) win->data;
	struct widget_data *di = selected_widget(dlg_data);
	int cdata_len = strlen(di->cdata);
	int n = 0;
	struct input_history_item *hi;
	struct menu_item *items = NULL;

	foreach (hi, *history) {
		if (strncmp(di->cdata, hi->d, cdata_len)) continue;

		if (!realloc_menu_items(&items, n)) {
			if (items) mem_free(items);
			return;
		}

		SET_MENU_ITEM(&items[n], hi->d, "", tab_compl, hi->d, FREE_LIST, 0, 1, HKS_SHOW, 0);
		n++;
	}

	if (n) {
		if (n == 1) {
			tab_compl(term, items->data, win);
			mem_free(items);
			return;
		}

		memset(&items[n], 0, sizeof(struct menu_item));
		do_menu_selected(term, items, win, n - 1, 0);
	}
}

/* Complete to the last unambiguous character. Eg., I've been to google.com,
 * google.com/search?q=foo, and google.com/search?q=bar.  This function then
 * completes `go' to `google.com' and `google.com/' to `google.com/search?q='.
 */
void
do_tab_compl_unambiguous(struct terminal *term, struct list_head *history,
			 struct window *win)
{
	struct dialog_data *dlg_data = (struct dialog_data *) win->data;
	struct widget_data *di = selected_widget(dlg_data);
	int cdata_len = strlen(di->cdata);
	int match_len = cdata_len;
	/* Maximum number of characters in a match. Characters after this
	 * position are varying in other matches. Zero means that no max has
	 * been set yet. */
	int max = 0;
	unsigned char *match = NULL;
	struct input_history_item *cur;

	foreach (cur, *history) {
		unsigned char *c = cur->d - 1;
		unsigned char *m = (match ? match : di->cdata) - 1;
		int len = 0;

		while (*++m && *++c && *m == *c && (++len, !max || len < max));
		if (len < cdata_len)
			continue;
		if (len < match_len || (*c && m != di->cdata + len))
			max = len;
		match = cur->d;
		match_len = (m == di->cdata + len && !*m) ? strlen(cur->d) : len;
	}

	if (!match) return;

	tab_compl_n(term, match, match_len, win);
}


/* Search duplicate entries in history list and remove older ones. */
static void
remove_duplicate_from_history(struct input_history *historylist,
			      unsigned char *url)
{
	struct input_history_item *historyitem;

	if (!historylist || !url || !*url) return;

	foreach (historyitem, historylist->items) {
		struct input_history_item *tmphistoryitem;

		if (strcmp(historyitem->d, url)) continue;

		/* found a duplicate -> remove it from history list */

		tmphistoryitem = historyitem;
		historyitem = historyitem->prev;

		del_from_list(tmphistoryitem);
		mem_free(tmphistoryitem);

		historylist->n--;
	}
}

/* Add a new entry in inputbox history list, take care of duplicate if
 * check_duplicate and respect history size limit. */
void
add_to_input_history(struct input_history *historylist, unsigned char *url,
		     int check_duplicate)
{
	struct input_history_item *newhistoryitem;
	int url_len;

	if (!historylist || !url)
		return;

	/* Strip spaces at the margins */
	trim_chars(url, ' ', &url_len);
	if (!url_len) return;

	/* Copy it all etc. */
	/* One byte is already reserved for url in struct input_history_item. */
	newhistoryitem = mem_alloc(sizeof(struct input_history_item) + url_len);
	if (!newhistoryitem) return;

	memcpy(newhistoryitem->d, url, url_len + 1);

	if (check_duplicate)
		remove_duplicate_from_history(historylist, newhistoryitem->d);

	/* add new entry to history list */
	add_to_list(historylist->items, newhistoryitem);
	historylist->n++;

	if (!history_nosave) history_dirty = 1;

	/* limit size of history to MAX_HISTORY_ITEMS
	 * removing first entries if needed */
	while (historylist->n > MAX_HISTORY_ITEMS) {
		struct input_history_item *tmphistoryitem = historylist->items.prev;

		if ((void *) tmphistoryitem == &historylist->items) {
			internal("history is empty");
			historylist->n = 0;
			return;
		}

		del_from_list(tmphistoryitem);
		mem_free(tmphistoryitem);
		historylist->n--;
	}
}

/* Load history file */
int
load_input_history(struct input_history *history, unsigned char *filename)
{
	unsigned char *history_file = filename;
	unsigned char line[MAX_STR_LEN];
	FILE *file;

	if (get_opt_int_tree(cmdline_options, "anonymous")) return 0;
	if (elinks_home) {
		history_file = straconcat(elinks_home, filename, NULL);
		if (!history_file) return 0;
	}

	file = fopen(history_file, "r");
	if (elinks_home) mem_free(history_file);
	if (!file) return 0;

	while (safe_fgets(line, MAX_STR_LEN, file)) {
		/* Drop '\n'. */
		if (*line) line[strlen(line) - 1] = 0;
		add_to_input_history(history, line, 0);
	}

	fclose(file);
	return 0;
}

/* Write history list to file. It returns a value different from 0 in case of
 * failure, 0 on success. */
int
save_input_history(struct input_history *history, unsigned char *filename)
{
	struct input_history_item *historyitem;
	struct secure_save_info *ssi;
	unsigned char *history_file;
	int i = 0;

	if (!elinks_home
	    || get_opt_int_tree(cmdline_options, "anonymous"))
		return 0;

	history_file = straconcat(elinks_home, filename, NULL);
	if (!history_file) return -1;

	ssi = secure_open(history_file, 0177);
	mem_free(history_file);
	if (!ssi) return -1;

	foreachback (historyitem, history->items) {
		if (i++ > MAX_HISTORY_ITEMS) break;
		secure_fputs(ssi, historyitem->d);
		secure_fputc(ssi, '\n');
		if (ssi->err) break;
	}

	return secure_close(ssi);
}
