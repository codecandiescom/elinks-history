/* Global history dialogs */
/* $Id: dialogs.c,v 1.20 2003/05/02 22:39:15 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include <string.h>

#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "dialogs/edit.h"
#include "dialogs/hierbox.h"
#include "globhist/dialogs.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/kbd.h"
#include "util/memory.h"
#include "util/string.h"


#ifdef GLOBHIST

#define HISTORY_BOX_IND 7

struct history_dialog_list_item {
	LIST_HEAD(struct history_dialog_list_item);
	struct dialog_data *dlg;
};

static INIT_LIST_HEAD(history_dialog_list);

static void listbox_delete_historyitem(struct terminal *,
				       struct listbox_data *);

static struct listbox_ops gh_listbox_ops = {
	listbox_delete_historyitem,
};

void
update_all_history_dialogs(void)
{
	struct history_dialog_list_item *item;

	foreach (item, history_dialog_list) {
		display_dlg_item(item->dlg,
				 &(item->dlg->items[HISTORY_BOX_IND]), 1);
	}
}

/* Creates the box display (holds everything EXCEPT the actual rendering data) */
static struct listbox_data *
history_dialog_box_build()
{
	struct listbox_data *box;

	/* Deleted in abort */
	box = mem_calloc(1, sizeof(struct listbox_data));
	if (!box) return NULL;

	box->ops = &gh_listbox_ops;
	box->items = &gh_box_items;
	add_to_list(gh_boxes, box);

	return box;
}


/* Cleans up after the history dialog */
static void
history_dialog_abort_handler(struct dialog_data *dlg)
{
	struct listbox_data *box;
	struct history_dialog_list_item *item;

	box = (struct listbox_data *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	foreach (item, history_dialog_list) {
		if (item->dlg == dlg) {
			del_from_list(item);
			mem_free(item);
			break;
		}
	}

	del_from_list(box);

	mem_free(box);
}

static int
history_dialog_event_handler(struct dialog_data *dlg, struct event *ev)
{
	switch (ev->ev) {
		case EV_KBD:
			if (dlg->items[HISTORY_BOX_IND].item->ops->kbd)
				return dlg->items[HISTORY_BOX_IND].item->ops->kbd(&dlg->items[HISTORY_BOX_IND], dlg, ev);
			break;

		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
		case EV_MOUSE:
		case EV_ABORT:
			break;

		default:
			internal("Unknown event received: %d", ev->ev);
	}

	return EVENT_NOT_PROCESSED;
}


static void
history_search_do(struct dialog *d)
{
	struct listbox_item *item = gh_box_items.next;
	struct listbox_data *box;

	if (!globhist_simple_search(d->items[1].data, d->items[0].data)) return;
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
	do_edit_dialog(term, N_("Search history"), gh_last_searched_title,
		       gh_last_searched_url, ses, parent, history_search_do,
		       NULL, NULL, 0);
}

static int
push_search_button(struct dialog_data *dlg, struct widget_data *di)
{
	launch_search_dialog(dlg->win->term, dlg,
			     (struct session *) dlg->dlg->udata);
	return 0;
}

static int
push_toggle_display_button(struct dialog_data *dlg, struct widget_data *di)
{
	struct global_history_item *item;
	int *display_type;

	display_type = &get_opt_int("document.history.global.display_type");
	*display_type = !*display_type;

	foreach (item, global_history.items) {
		struct listbox_item *b2;
		unsigned char *text = *display_type ? item->title : item->url;

		if (!*text) text = item->url;

		b2 = mem_realloc(item->box_item,
				sizeof(struct listbox_item) + strlen(text) + 1);
		if (!b2) continue;

		if (b2 != item->box_item) {
			struct listbox_data *box;

			/* We are being relocated, so update everything. */
			/* If there'll be ever any hiearchy, this will have to
			 * be extended by root/child handling. */
			b2->next->prev = b2;
			b2->prev->next = b2;
			foreach (box, *b2->box) {
				if (box->sel == item->box_item) box->sel = b2;
				if (box->top == item->box_item) box->top = b2;
			}
			item->box_item = b2;
			item->box_item->text =
				((unsigned char *) item->box_item
				 + sizeof(struct listbox_item));
		}

		strcpy(item->box_item->text, text);
	}

	update_all_history_dialogs();

	return 0;
}


static int
push_goto_button(struct dialog_data *dlg, struct widget_data *goto_btn)
{
	struct global_history_item *historyitem;
	struct listbox_data *box;

	box = (struct listbox_data *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	/* Follow the history item */
	if (box->sel) {
		historyitem = box->sel->udata;
		if (historyitem)
			goto_url((struct session *) goto_btn->item->udata,
				 historyitem->url);
	}

	/* Close the history dialog */
	delete_window(dlg->win);
	return 0;
}


struct delete_globhist_item_ctx {
	struct global_history_item *history_item;
	struct terminal *term;
};

static void
do_delete_global_history_item(struct terminal *term,
			      struct global_history_item *historyitem)
{
	if (historyitem->refcount > 0) {
		msg_box(term, NULL,
			N_("Delete history item"), AL_CENTER | AL_EXTD_TEXT,
			N_("Sorry, but this bookmark is already being used by something right now."), "\n\n",
			N_("Title"), ": \"", historyitem->title, "\n",
			N_("URL"), ": \"", historyitem->url, NULL,
 			NULL, 1,
 			N_("Cancel"), NULL, B_ENTER | B_ESC);
 		return;
 	}

	delete_global_history_item(historyitem);
}

static int
delete_marked(struct listbox_item *item, void *data_, int *offset)
{
	struct delete_globhist_item_ctx *vhop = data_;

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
	struct delete_globhist_item_ctx *ctx = vhop;

	if (ctx->history_item) {
		if (ctx->history_item->refcount)
			ctx->history_item->refcount--;
		do_delete_global_history_item(ctx->term, ctx->history_item);
	} else {
		traverse_listbox_items_list(gh_box_items.next, 0, 0,
						delete_marked, ctx);
	}
 }

static void
cancel_delete_globhist_item(void *vhop)
{
	struct delete_globhist_item_ctx *ctx = vhop;

	if (ctx->history_item) ctx->history_item->refcount--;
}

static int
scan_for_marks(struct listbox_item *item, void *data_, int *offset)
{
	if (item->marked) {
		struct delete_globhist_item_ctx *ctx = data_;

		ctx->history_item = NULL;
		*offset = 0;
	}
	return 0;
}

static void
listbox_delete_historyitem(struct terminal *term, struct listbox_data *box)
{
	struct delete_globhist_item_ctx *ctx;
	struct global_history_item *historyitem;

	if (!box->sel) return;
	historyitem = (struct global_history_item *) box->sel->udata;
	if (!historyitem) return;

 	ctx = mem_alloc(sizeof(struct delete_globhist_item_ctx));
	if (!ctx) return;

 	ctx->history_item = historyitem;
 	ctx->term = term;

	traverse_listbox_items_list(box->items->next, 0, 0,
				    scan_for_marks, ctx);
	historyitem = ctx->history_item;

	if (historyitem) historyitem->refcount++;

	if (!historyitem)
		msg_box(term, getml(ctx, NULL),
			N_("Delete history item"), AL_CENTER,
			N_("Delete marked history items?"),
			ctx, 2,
			N_("Yes"), really_delete_global_history_item, B_ENTER,
			N_("No"), cancel_delete_globhist_item, B_ESC);
	/* XXX: When we add tree-history, remember to add a check for
	 * historyitem->box_item->type == BI_FOLDER here. -- Miciah */
	else
		msg_box(term, getml(ctx, NULL),
			N_("Delete history item"), AL_CENTER | AL_EXTD_TEXT,
			N_("Delete history item"), " \"", historyitem->title, "\" ?\n\n",
			N_("URL"), ": \"", historyitem->url, "\"", NULL,
			ctx, 2,
			N_("Yes"), really_delete_global_history_item, B_ENTER,
			N_("No"), cancel_delete_globhist_item, B_ESC);

	return;
}


static int
push_delete_button(struct dialog_data *dlg,
		   struct widget_data *some_useless_delete_button)
{
	struct listbox_data *box;
	struct terminal *term = dlg->win->term;

	box = (struct listbox_data *) dlg->dlg->items[HISTORY_BOX_IND].data;
	listbox_delete_historyitem(term, box);
	return 0;
}


static void
really_clear_history(struct listbox_data *box)
{
	while (global_history.n) {
		if (((struct global_history_item *)
		     global_history.items.prev)->refcount > 0)
			break;
		delete_global_history_item(global_history.items.prev);
	}
}

static int
push_clear_button(struct dialog_data *dlg,
		  struct widget_data *some_useless_clear_button)
{
	struct terminal *term = dlg->win->term;
	struct listbox_data *box;

	box = (struct listbox_data *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	msg_box(term, NULL,
		N_("Clear global history"), AL_CENTER | AL_EXTD_TEXT,
		N_("Clear global history"), "?", NULL,
		box, 2,
		N_("Yes"), really_clear_history, B_ENTER,
		N_("No"), NULL, B_ESC);

	return 0;
}


static void
done_info_button(void *vhop)
{
	struct global_history_item *history_item = vhop;

	history_item->refcount--;
}

static int
push_info_button(struct dialog_data *dlg,
		  struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg->win->term;
	struct global_history_item *historyitem;
	struct listbox_data *box;

	box = (struct listbox_data *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	/* Show history item info */
	if (!box->sel) return 0;
	historyitem = box->sel->udata;
	if (!historyitem) return 0;
	historyitem->refcount++;

	msg_box(term, NULL,
		N_("Info"), AL_LEFT | AL_EXTD_TEXT,
		N_("Title"), ": ", historyitem->title, "\n",
		N_("URL"), ": ", historyitem->url, "\n",
		N_("Last visit time"), ": ", ctime(&historyitem->last_visit), NULL,
		historyitem, 1,
		N_("OK"), done_info_button, B_ESC | B_ENTER);

	return 0;
}



void
menu_history_manager(struct terminal *term, void *fcp, struct session *ses)
{
	struct dialog *d;
	struct dialog_data *dd;
	struct history_dialog_list_item *item;
	struct global_history_item *litem;

	foreach (litem, global_history.items) {
		litem->box_item->visible = 1;
	}

	if (gh_last_searched_title) {
		mem_free(gh_last_searched_title);
		gh_last_searched_title = NULL;
	}

	if (gh_last_searched_url) {
		mem_free(gh_last_searched_url);
		gh_last_searched_url = NULL;
	}

	/* XXX: sizeof(struct global_history_item): why? */

	d = mem_calloc(1, sizeof(struct dialog)
		          + (HISTORY_BOX_IND + 2) * sizeof(struct widget)
		          + sizeof(struct global_history_item)
			  + 2 * MAX_STR_LEN);
	if (!d) return;

	d->title = N_("Global history");
	d->fn = layout_hierbox_browser;
	d->handle_event = history_dialog_event_handler;
	d->abort = history_dialog_abort_handler;
	d->udata = ses;

	d->items[0].type = D_BUTTON;
	d->items[0].gid = B_ENTER;
	d->items[0].fn = push_goto_button;
	d->items[0].udata = ses;
	d->items[0].text = N_("Goto");

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = push_info_button;
	d->items[1].text = N_("Info");

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = push_delete_button;
	d->items[2].text = N_("Delete");

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ENTER;
	d->items[3].fn = push_search_button;
	d->items[3].text = N_("Search");

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ENTER;
	d->items[4].fn = push_toggle_display_button;
	d->items[4].text = N_("Toggle display");

	d->items[5].type = D_BUTTON;
	d->items[5].gid = B_ENTER;
	d->items[5].fn = push_clear_button;
	d->items[5].text = N_("Clear");

	d->items[6].type = D_BUTTON;
	d->items[6].gid = B_ESC;
	d->items[6].fn = cancel_dialog;
	d->items[6].text = N_("Close");

	d->items[HISTORY_BOX_IND].type = D_BOX;
	d->items[HISTORY_BOX_IND].gid = 12;
	d->items[HISTORY_BOX_IND].data = (void *) history_dialog_box_build();

	d->items[HISTORY_BOX_IND + 1].type = D_END;
	dd = do_dialog(term, d, getml(d, NULL));

	item = mem_alloc(sizeof(struct history_dialog_list_item));
	if (item) {
		item->dlg = dd;
		add_to_list(history_dialog_list, item);
	}
}

#endif /* GLOBHIST */
