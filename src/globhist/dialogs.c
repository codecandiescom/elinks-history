/* Global history dialogs */
/* $Id: dialogs.c,v 1.78 2003/11/22 02:40:02 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include <string.h>

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


#ifdef GLOBHIST

static void listbox_delete_historyitem(struct terminal *,
				       struct listbox_data *);

static struct listbox_ops gh_listbox_ops = {
	listbox_delete_historyitem,
};

struct hierbox_browser globhist_browser = {
	{ D_LIST_HEAD(globhist_browser.boxes) },
	&gh_box_items,
	{ D_LIST_HEAD(globhist_browser.dialogs) },
	&gh_listbox_ops,
};


static void
history_search_do(struct dialog *dlg)
{
	struct listbox_item *item = gh_box_items.next;
	struct listbox_data *box;

	if (!globhist_simple_search(dlg->widgets[1].data, dlg->widgets[0].data)) return;
	if (list_empty(gh_box_items)) return;

	foreach (box, *item->box) {
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


static int
push_goto_button(struct dialog_data *dlg_data, struct widget_data *goto_btn)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct global_history_item *historyitem;

	/* Follow the history item */
	if (box->sel) {
		historyitem = box->sel->udata;
		if (historyitem)
			goto_url((struct session *) goto_btn->widget->udata,
				 historyitem->url);
	}

	/* Close the history dialog */
	delete_window(dlg_data->win);
	return 0;
}


static void
do_delete_global_history_item(struct terminal *term,
			      struct global_history_item *historyitem)
{
	if (is_object_used(historyitem)) {
		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("Delete history item"), AL_CENTER,
			msg_text(term, N_("Sorry, but this history entry is "
				"already being used by something right now.\n\n"
				"Title: \"%s\"\n"
				"URL: \"%s\"\n"),
				historyitem->title, historyitem->url),
 			NULL, 1,
 			N_("OK"), NULL, B_ENTER | B_ESC);
 		return;
 	}

	delete_global_history_item(historyitem);
}

static int
delete_marked(struct listbox_item *item, void *data_, int *offset)
{
	struct delete_hierbox_item_info *vhop = data_;

	if (item->marked) {
		do_delete_global_history_item(vhop->term,
				(struct global_history_item *) item->udata);
		return 1;
	}
	return 0;
}

static void
really_delete_global_history_item(void *vhop)
{
	struct delete_hierbox_item_info *ctx = vhop;

	if (ctx->item) {
		struct global_history_item *item = ctx->item->udata;

		if (is_object_used(item))
			object_unlock(item);
		do_delete_global_history_item(ctx->term, item);
	} else {
		traverse_listbox_items_list(gh_box_items.next, 0, 0,
						delete_marked, ctx);
	}
}

static void
cancel_delete_globhist_item(void *vhop)
{
	struct delete_hierbox_item_info *ctx = vhop;

	if (ctx->item)
		object_unlock((struct global_history_item *)ctx->item->udata);
}

static void
listbox_delete_historyitem(struct terminal *term, struct listbox_data *box)
{
	struct delete_hierbox_item_info *ctx;

	if (!box->sel || !box->sel->udata) return;

 	ctx = get_hierbox_delete_info(box, term);

	if (!ctx->item) {
		msg_box(term, getml(ctx, NULL), 0,
			N_("Delete history item"), AL_CENTER,
			N_("Delete marked history items?"),
			ctx, 2,
			N_("Yes"), really_delete_global_history_item, B_ENTER,
			N_("No"), cancel_delete_globhist_item, B_ESC);
	/* XXX: When we add tree-history, remember to add a check for
	 * historyitem->box_item->type == BI_FOLDER here. -- Miciah */
	} else {
		struct global_history_item *historyitem = ctx->item->udata;

		object_lock(historyitem);

		msg_box(term, getml(ctx, NULL), MSGBOX_FREE_TEXT,
			N_("Delete history item"), AL_CENTER,
			msg_text(term, N_("Delete history item \"%s\"?\n\n"
				"URL: \"%s\""),
				historyitem->title, historyitem->url),
			ctx, 2,
			N_("Yes"), really_delete_global_history_item, B_ENTER,
			N_("No"), cancel_delete_globhist_item, B_ESC);
	}

	return;
}


static void
really_clear_history(struct listbox_data *box)
{
	while (global_history.n) {
		if (is_object_used(
		    (struct global_history_item *) global_history.items.prev))
			break;
		delete_global_history_item(global_history.items.prev);
	}
}

static int
push_clear_button(struct dialog_data *dlg_data,
		  struct widget_data *some_useless_clear_button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;

	msg_box(term, NULL, 0,
		N_("Clear global history"), AL_CENTER,
		N_("Clear global history?"),
		box, 2,
		N_("Yes"), really_clear_history, B_ENTER,
		N_("No"), NULL, B_ESC);

	return 0;
}


static void
done_info_button(void *vhop)
{
	struct global_history_item *history_item = vhop;

	object_unlock(history_item);
}

static int
push_info_button(struct dialog_data *dlg_data,
		  struct widget_data *some_useless_info_button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct global_history_item *historyitem;

	/* Show history item info */
	if (!box->sel) return 0;
	historyitem = box->sel->udata;
	if (!historyitem) return 0;
	object_lock(historyitem);

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("Info"), AL_LEFT,
		msg_text(term, N_("Title: %s\n"
			"URL: %s\n"
			"Last visit time: %s"),
			historyitem->title, historyitem->url,
			ctime(&historyitem->last_visit)),
		historyitem, 1,
		N_("OK"), done_info_button, B_ESC | B_ENTER);

	return 0;
}


#ifdef BOOKMARKS
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

#ifdef BOOKMARKS
# define GLOBHIST_MANAGER_BUTTONS	7
#else
# define GLOBHIST_MANAGER_BUTTONS	6
#endif

#define GLOBHIST_MANAGER_ADDSIZE	(sizeof(struct global_history_item) + 2 * MAX_STR_LEN)

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

	hierbox_browser(term, N_("Global history"),
			GLOBHIST_MANAGER_ADDSIZE, &globhist_browser, ses,
			GLOBHIST_MANAGER_BUTTONS,
			N_("Goto"), push_goto_button, B_ENTER, ses,
			N_("Info"), push_info_button, B_ENTER, ses,
#ifdef BOOKMARKS
			N_("Bookmark"), push_bookmark_button, B_ENTER, NULL,
#endif
			N_("Delete"), push_hierbox_delete_button, B_ENTER, NULL,
			N_("Search"), push_search_button, B_ENTER, NULL,
			N_("Toggle display"), push_toggle_display_button, B_ENTER, ses,
			N_("Clear"), push_clear_button, B_ENTER, NULL);
}

#endif /* GLOBHIST */
