/* Global history dialogs */
/* $Id: dialogs.c,v 1.101 2004/01/01 14:24:08 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_GLOBHIST

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "bookmarks/dialogs.h"
#include "dialogs/edit.h"
#include "globhist/dialogs.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/object.h"


/* Implementation of the listbox operations */

static void lock_globhist_item(struct listbox_item *item)
{
	object_lock((struct global_history_item *)item->udata);
}

static void unlock_globhist_item(struct listbox_item *item)
{
	object_unlock((struct global_history_item *)item->udata);
}

static int is_globhist_item_used(struct listbox_item *item)
{
	return is_object_used((struct global_history_item *)item->udata);
}

static unsigned char *
get_globhist_item_info(struct listbox_item *box_item, struct terminal *term,
		       enum listbox_info listbox_info)
{
	struct global_history_item *item = box_item->udata;
	struct string info;

	if (listbox_info == LISTBOX_URI)
		return stracpy(item->url);

	if (!init_string(&info)) return NULL;

	add_format_to_string(&info, "%s: %s", _("Title", term), item->title);
	add_format_to_string(&info, "\n%s: %s", _("URL", term), item->url);
	add_format_to_string(&info, "\n%s: %s", _("Last visit time", term),
				ctime(&item->last_visit));

	return info.source;
}

static int
can_delete_globhist_item(struct listbox_item *item)
{
	return 1;
}

static void
delete_globhist_item(struct listbox_item *item, int last)
{
	struct global_history_item *historyitem = item->udata;

	assert(!is_object_used(historyitem));

	delete_global_history_item(historyitem);
}

static struct listbox_ops gh_listbox_ops = {
	lock_globhist_item,
	unlock_globhist_item,
	is_globhist_item_used,
	get_globhist_item_info,
	can_delete_globhist_item,
	delete_globhist_item,
	NULL,
};

/* Searching: */

static void
history_search_do(struct dialog *dlg)
{
	struct listbox_item *item = gh_box_items.next;
	struct listbox_data *box;

	if (!globhist_simple_search(dlg->widgets[1].data, dlg->widgets[0].data)) return;
	if (list_empty(gh_box_items)) return;

	/* Shouldn't we rather do this only for the specific listbox_data box
	 * in dlg->widget->data so only the current dialog is updated? --jonas */
	foreach (box, globhist_browser.boxes) {
		box->top = item;
		box->sel = box->top;
	}
}

static void
launch_search_dialog(struct terminal *term, struct dialog_data *parent,
		     struct session *ses)
{
	do_edit_dialog(term, 1, N_("Search history"), gh_last_searched_title,
		       gh_last_searched_url, ses, parent, history_search_do,
		       NULL, NULL, EDIT_DLG_SEARCH);
}

static int
push_search_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	launch_search_dialog(dlg_data->win->term, dlg_data,
			     (struct session *) dlg_data->dlg->udata);
	return 0;
}

/* Toggling: */

static int
push_toggle_display_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct global_history_item *item;
	int *display_type;

	display_type = (int *) &get_opt_int("document.history.global.display_type");
	*display_type = !*display_type;

	foreach (item, global_history.items) {
		unsigned char *text = *display_type ? item->title : item->url;

		if (!*text) text = item->url;
		item->box_item->text = text;
	}

	update_hierbox_browser(&globhist_browser);

	return 0;
}

/* Bookmarking: */

#ifdef CONFIG_BOOKMARKS
static int
push_bookmark_button(struct dialog_data *dlg_data,
		     struct widget_data *some_useless_info_button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct global_history_item *historyitem;

	if (!box->sel) return 0;

	historyitem = box->sel->udata;
	if (!historyitem) return 0;

	launch_bm_add_dialog(term, NULL, NULL,
			     historyitem->title, historyitem->url);

	return 0;
}
#endif

/* The global history manager: */

static struct hierbox_browser_button globhist_buttons[] = {
	{ N_("Goto"),		push_hierbox_goto_button,	1 },
	{ N_("Info"),		push_hierbox_info_button,	1 },
#ifdef CONFIG_BOOKMARKS
	{ N_("Bookmark"),	push_bookmark_button,		0 },
#endif
	{ N_("Delete"),		push_hierbox_delete_button,	1 },
	{ N_("Search"),		push_search_button,		1 },
	{ N_("Toggle display"),	push_toggle_display_button,	1 },
	{ N_("Clear"),		push_hierbox_clear_button,	1 },
#if 0
	/* TODO: Would this be useful? --jonas */
	{ N_("Save"),		push_save_button		},
#endif
};

struct hierbox_browser globhist_browser = {
	N_("Global history manager"),
	globhist_buttons,
	HIERBOX_BROWSER_BUTTONS_SIZE(globhist_buttons),

	{ D_LIST_HEAD(globhist_browser.boxes) },
	&gh_box_items,
	{ D_LIST_HEAD(globhist_browser.dialogs) },
	&gh_listbox_ops,
};

void
menu_history_manager(struct terminal *term, void *fcp, struct session *ses)
{
	if (gh_last_searched_title) {
		mem_free(gh_last_searched_title);
		gh_last_searched_title = NULL;
	}

	if (gh_last_searched_url) {
		mem_free(gh_last_searched_url);
		gh_last_searched_url = NULL;
	}

	hierbox_browser(&globhist_browser, ses);
}

#endif /* CONFIG_GLOBHIST */
