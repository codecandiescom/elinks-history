/* Input history for input fields. */
/* $Id: inphist.c,v 1.27 2003/09/22 14:59:13 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/menu.h"
#include "config/urlhist.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/lists.h"
#include "util/memory.h"


static void
tab_compl_n(struct terminal *term, unsigned char *item, int len,
	    struct window *win)
{
	struct event ev = {EV_REDRAW, term->x, term->y, 0};
	struct dialog_data *dd = (struct dialog_data *) win->data;
	struct widget_data *di = &(dd)->items[dd->selected];

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

#define realloc_menu_items(menu, size) \
	mem_align_alloc(menu, size, (size) + 1, sizeof(struct menu_item), 0xFF)

/* Complete to last unambiguous character, and display menu for all possible
 * further completions. */
void
do_tab_compl(struct terminal *term, struct list_head *history,
	     struct window *win)
{
	struct dialog_data *dd = (struct dialog_data *) win->data;
	unsigned char *cdata = dd->items[dd->selected].cdata;
	int l = strlen(cdata);
	int n = 0;
	struct input_history_item *hi;
	struct menu_item *items = NULL;

	foreach (hi, *history) {
		if (strncmp(cdata, hi->d, l)) continue;

		if (!realloc_menu_items(&items, n)) {
			if (items) mem_free(items);
			return;
		}

		items[n].text = hi->d;
		items[n].rtext = "";
		items[n].func = (void(*)(struct terminal *, void *, void *))tab_compl;
		items[n].data = hi->d;
		items[n].item_free = FREE_LIST;
		items[n].submenu = 0;
		items[n].no_intl = 1;
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
	struct dialog_data *dd = (struct dialog_data *) win->data;
	unsigned char *cdata = dd->items[dd->selected].cdata;
	int cdata_len = strlen(cdata);
	int match_len = cdata_len;
	/* Maximum number of characters in a match. Characters after this
	 * position are varying in other matches. Zero means that no max has
	 * been set yet. */
	int max = 0;
	unsigned char *match = NULL;
	struct input_history_item *cur;

	foreach (cur, *history) {
		unsigned char *c = cur->d - 1;
		unsigned char *m = (match ? match : cdata) - 1;
		int len = 0;

		while (*++m && *++c && *m == *c && (++len, !max || len < max));
		if (len < cdata_len)
			continue;
		if (len < match_len || (*c && m != cdata + len))
			max = len;
		match = cur->d;
		match_len = (m == cdata + len && !*m) ? strlen(cur->d) : len;
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
