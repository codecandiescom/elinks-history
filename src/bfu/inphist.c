/* Input history for input fields. */
/* $Id: inphist.c,v 1.84 2004/04/16 10:02:06 zas Exp $ */

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
#include "lowlevel/home.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/lists.h"
#include "util/math.h"
#include "util/memory.h"
#include "util/secsave.h"


static void
tab_compl_n(struct dialog_data *dlg_data, unsigned char *item, int len)
{
	struct widget_data *widget_data = selected_widget(dlg_data);

	assert(widget_is_textfield(widget_data));

	int_upper_bound(&len, widget_data->widget->datalen - 1);
	memcpy(widget_data->cdata, item, len);
	widget_data->cdata[len] = 0;
	widget_data->info.field.cpos = len;
	widget_data->info.field.vpos = 0;

	redraw_dialog(dlg_data, 1);
}

static inline void
tab_compl(struct terminal *term, unsigned char *item, struct dialog_data *dlg_data)
{
	tab_compl_n(dlg_data, item, strlen(item));
}

/* Complete to last unambiguous character, and display menu for all possible
 * further completions. */
void
do_tab_compl(struct dialog_data *dlg_data, struct list_head *history)
{
	struct terminal *term = dlg_data->win->term;
	struct widget_data *widget_data = selected_widget(dlg_data);
	int cdata_len = strlen(widget_data->cdata);
	int n = 0;
	struct input_history_entry *entry;
	struct menu_item *items = new_menu(FREE_LIST | NO_INTL);

	if (!items) return;

	foreach (entry, *history) {
		if (strncmp(widget_data->cdata, entry->data, cdata_len))
			continue;

		add_to_menu(&items, entry->data, NULL, ACT_MAIN_NONE,
			    (menu_func) tab_compl, entry->data, 0);
		n++;
	}

	if (n > 1) {
		do_menu_selected(term, items, dlg_data, n - 1, 0);
	} else {
		if (n == 1) tab_compl(term, items->data, dlg_data);
		mem_free(items);
	}
}

/* Complete to the last unambiguous character. Eg., I've been to google.com,
 * google.com/search?q=foo, and google.com/search?q=bar.  This function then
 * completes `go' to `google.com' and `google.com/' to `google.com/search?q='.
 */
void
do_tab_compl_unambiguous(struct dialog_data *dlg_data, struct list_head *history)
{
	struct widget_data *widget_data = selected_widget(dlg_data);
	int base_len = strlen(widget_data->cdata);
	/* Maximum number of characters in a match. Characters after this
	 * position are varying in other matches. Zero means that no max has
	 * been set yet. */
	int longest_common_match = 0;
	unsigned char *match = NULL;
	struct input_history_entry *entry;

	foreach (entry, *history) {
		unsigned char *cur = entry->data;
		unsigned char *matchpos = match ? match : widget_data->cdata;
		int cur_len = 0;

		for (; *cur && *cur == *matchpos; ++cur, ++matchpos) {
			++cur_len;

			/* XXX: I think that unifying the two cases of this
			 * test could seriously hurt readability. --pasky */
			if (longest_common_match
			    && cur_len >= longest_common_match)
				break;
		}

		if (cur_len < base_len)
			continue;

		if (!match) cur_len = strlen(entry->data);

		/* By now, @cur_len oscillates between @base_len and
		 * @longest_common_match. */
		if (longest_common_match
		    && cur_len >= longest_common_match)
			continue;

		/* We found the next shortest common match. */
		longest_common_match = cur_len;
		match = entry->data;
	}

	if (!match) return;

	tab_compl_n(dlg_data, match, longest_common_match);
}


/* Search for duplicate entries in history list, save first one and remove
 * older ones. */
static struct input_history_entry *
check_duplicate_entries(struct input_history *history, unsigned char *data)
{
	struct input_history_entry *entry, *first_duplicate = NULL;

	if (!history || !data || !*data) return NULL;

	foreach (entry, history->entries) {
		struct input_history_entry *duplicate;

		if (strcmp(entry->data, data)) continue;

		/* Found a duplicate -> remove it from history list */

		duplicate = entry;
		entry = entry->prev;

		del_from_history_list(history, duplicate);

		/* Save the first duplicate entry */
		if (!first_duplicate) {
			first_duplicate = duplicate;
		} else {
			mem_free(duplicate);
		}
	}

	return first_duplicate;
}

/* Add a new entry in inputbox history list, take care of duplicate if
 * check_duplicate and respect history size limit. */
void
add_to_input_history(struct input_history *history, unsigned char *data,
		     int check_duplicate)
{
	struct input_history_entry *entry;
	int length;

	if (!history || !data || !*data)
		return;

	/* Strip spaces at the margins */
	trim_chars(data, ' ', &length);
	if (!length) return;

	if (check_duplicate) {
		entry = check_duplicate_entries(history, data);
		if (entry) {
			add_to_history_list(history, entry);
			return;
		}
	}

	/* Copy it all etc. */
	/* One byte is already reserved for url in struct input_history_item. */
	entry = mem_alloc(sizeof(struct input_history_entry) + length);
	if (!entry) return;

	memcpy(entry->data, data, length + 1);

	add_to_history_list(history, entry);

	/* limit size of history to MAX_INPUT_HISTORY_ENTRIES
	 * removing first entries if needed */
	while (history->size > MAX_INPUT_HISTORY_ENTRIES) {
		if (list_empty(history->entries)) {
			INTERNAL("history is empty");
			history->size = 0;
			return;
		}

		entry = history->entries.prev;
		del_from_history_list(history, entry);
		mem_free(entry);
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

	history->nosave = 1;

	file = fopen(history_file, "r");
	if (elinks_home) mem_free(history_file);
	if (!file) return 0;

	while (fgets(line, MAX_STR_LEN, file)) {
		/* Drop '\n'. */
		if (*line) line[strlen(line) - 1] = 0;
		add_to_input_history(history, line, 0);
	}

	fclose(file);
	history->nosave = 0;

	return 0;
}

/* Write history list to file. It returns a value different from 0 in case of
 * failure, 0 on success. */
int
save_input_history(struct input_history *history, unsigned char *filename)
{
	struct input_history_entry *entry;
	struct secure_save_info *ssi;
	unsigned char *history_file;
	int i = 0;

	if (!history->dirty
	    || !elinks_home
	    || get_opt_int_tree(cmdline_options, "anonymous"))
		return 0;

	history_file = straconcat(elinks_home, filename, NULL);
	if (!history_file) return -1;

	ssi = secure_open(history_file, 0177);
	mem_free(history_file);
	if (!ssi) return -1;

	foreachback (entry, history->entries) {
		if (i++ > MAX_INPUT_HISTORY_ENTRIES) break;
		secure_fputs(ssi, entry->data);
		secure_fputc(ssi, '\n');
		if (ssi->err) break;
	}

	if (!ssi->err) history->dirty = 0;

	return secure_close(ssi);
}
