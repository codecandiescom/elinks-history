/* Cache-related dialogs */
/* $Id: dialogs.c,v 1.5 2003/11/17 18:22:24 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "cache/cache.h"
#include "cache/dialogs.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/string.h"


struct cache_dialog_list_item {
	LIST_HEAD(struct cache_dialog_list_item);
	struct dialog_data *dlg_data;
};

static INIT_LIST_HEAD(cache_dialog_list);

void
update_all_cache_dialogs(void)
{
	struct cache_dialog_list_item *item;

	foreach (item, cache_dialog_list) {
		struct widget_data *widget_data =
			item->dlg_data->widgets_data;

		display_dlg_item(item->dlg_data, widget_data, 1);
	}
}

/* Creates the box display (holds everything EXCEPT the actual rendering data) */
static struct listbox_data *
cache_dialog_box_build(void)
{
	struct listbox_data *box;

	/* Deleted in abort */
	box = mem_calloc(1, sizeof(struct listbox_data));
	if (!box) return NULL;

	box->items = &cache_entry_box_items;
	add_to_list(cache_entry_boxes, box);

	return box;
}


/* Cleans up after the cache dialog */
static void
cache_dialog_abort_handler(struct dialog_data *dlg_data)
{
	struct cache_dialog_list_item *item;

	foreach (item, cache_dialog_list) {
		if (item->dlg_data == dlg_data) {
			del_from_list(item);
			mem_free(item);
			break;
		}
	}
}



static void
done_info_button(void *vhop)
{
	struct cache_entry *ce = vhop;

	cache_entry_unlock(ce);
}

static int
push_info_button(struct dialog_data *dlg_data,
		  struct widget_data *some_useless_info_button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct cache_entry *ce;
	struct string msg;

	/* Show history item info */
	if (!box->sel) return 0;
	ce = box->sel->udata;
	if (!ce) return 0;

	cache_entry_lock(ce);

	/* TODO: More info */

	add_to_string(&msg, _("URL", term));
	add_to_string(&msg, ": ");

	/* Add the uri with password and post info stripped */
	add_uri_to_string(&msg, &ce->uri, ~(URI_PASSWORD | URI_POST));

	add_format_to_string(&msg, "\n%s: %s", _("Last modified time", term),
						ce->last_modified);
	add_format_to_string(&msg, "\n%s: %d", _("Size", term), ce->length);
	add_format_to_string(&msg, "\n%s: %d", _("Loaded size", term), ce->data_size);
	if (ce->ssl_info) {
		add_format_to_string(&msg, "\n%s: %s", _("SSL Cipher", term),
						ce->ssl_info);
	}
	if (ce->encoding_info) {
		add_format_to_string(&msg, "\n%s: %s", _("Encoding", term),
						ce->encoding_info);
	}
	
	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("Info"), AL_LEFT,
		msg.source,
		ce, 1,
		N_("OK"), done_info_button, B_ESC | B_ENTER);

	return 0;
}


void
menu_cache_manager(struct terminal *term, void *fcp, struct session *ses)
{
	struct dialog_data *dlg_data;
	struct cache_dialog_list_item *item;
	struct listbox_item *litem;

	foreach (litem, cache_entry_box_items) {
		litem->visible = 1;
	}

	dlg_data = hierbox_browser(term, N_("Cache"),
			0, cache_dialog_box_build(), ses,
			1,
			N_("Info"), push_info_button, B_ENTER, ses);

	if (!dlg_data) return;
	dlg_data->dlg->abort = cache_dialog_abort_handler;

	item = mem_alloc(sizeof(struct cache_dialog_list_item));
	if (item) {
		item->dlg_data = dlg_data;
		add_to_list(cache_dialog_list, item);
	}
}
