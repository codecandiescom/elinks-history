/* Hiearchic listboxes browser dialog commons */
/* $Id: hierbox.c,v 1.103 2003/11/26 11:29:40 miciah Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "intl/gettext/libintl.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"


struct hierbox_dialog_list_item {
	LIST_HEAD(struct hierbox_dialog_list_item);

	struct dialog_data *dlg_data;
};

void
update_hierbox_browser(struct hierbox_browser *browser)
{
	struct hierbox_dialog_list_item *item;

	foreach (item, browser->dialogs) {
		struct widget_data *widget_data =
			item->dlg_data->widgets_data;

		display_dlg_item(item->dlg_data, widget_data, 1);
	}
}

struct listbox_item *
init_browser_box_item(struct hierbox_browser *browser, unsigned char *text,
		      void *data)
{
	struct listbox_item *item = mem_calloc(1, sizeof(struct listbox_item));

	if (!item) return NULL;

	init_list(item->child);
	item->visible = 1;

	item->text = text;
	item->udata = data;

	add_to_list(*browser->items, item);

	update_hierbox_browser(browser);

	return item;
}

/* Find a box to replace @item. This is done by trying first to traverse down, then
 * up and if both traversals end up returning the box we want to replace bail
 * out using NULL. */
static inline struct listbox_item *
replace_box_item(struct listbox_item *item, struct listbox_data *data)
{
	struct listbox_item *box;

	box = traverse_listbox_items_list(item, data, 1, 1, NULL, NULL);
	if (item != box) return box;

	box = traverse_listbox_items_list(item, data, -1, 1, NULL, NULL);
	return (item == box) ? NULL : box;
}

void
done_browser_box(struct hierbox_browser *browser, struct listbox_item *box_item)
{
	struct listbox_data *box_data;

	assert(box_item);

	/* If we are removing the top or the selected box we have to figure out
	 * a replacement. */

	foreach (box_data, browser->boxes) {
		if (box_data->sel == box_item)
			box_data->sel = replace_box_item(box_item, box_data);

		if (box_data->top == box_item)
			box_data->top = replace_box_item(box_item, box_data);
	}

	/* The option dialog needs this test */
	if (box_item->next) del_from_list(box_item);
	mem_free(box_item);
	update_hierbox_browser(browser);
}


static void
recursively_set_expanded(struct listbox_item *box, int expanded)
{
	struct listbox_item *child;

	if (box->type != BI_FOLDER)
		return;

	box->expanded = expanded;

	foreach (child, box->child)
		recursively_set_expanded(child, expanded);
}


struct ctx {
	struct listbox_item *item;
	int offset;
};

static int
test_search(struct listbox_item *item, void *data_, int *offset) {
	struct ctx *ctx = data_;

	ctx->offset--;

	if (item == ctx->item) *offset = 0;
	return 0;
}


/* We install own dialog event handler, so that we can give the listbox widget
 * an early chance to catch the event. Basically, the listbox widget is itself
 * unselectable, instead one of the buttons below is always active. So, we
 * always first let the listbox catch the keypress and handle it, and if it
 * doesn't care, we pass it on to the button. */
static int
hierbox_dialog_event_handler(struct dialog_data *dlg_data, struct term_event *ev)
{
	switch (ev->ev) {
		case EV_KBD:
		{
			struct widget_data *widget_data =
						dlg_data->widgets_data;
			struct widget *widget = widget_data->widget;
			struct listbox_data *box;
			struct listbox_item *selected;

			/* Check if listbox has something to say to this */
                        if (widget->ops->kbd
			    && widget->ops->kbd(widget_data, dlg_data, ev)
			       == EVENT_PROCESSED)
				return EVENT_PROCESSED;

			box = get_dlg_listbox_data(dlg_data);
			selected = box->sel;

			if (ev->x == ' ') {
				if (!selected) return EVENT_PROCESSED;
				if (selected->type != BI_FOLDER)
					return EVENT_NOT_PROCESSED;
				selected->expanded = !selected->expanded;
				break;
			}

			/* Recursively unexpand all folders */
			if (ev->x == '[' || ev->x == '-' || ev->x == '_') {
				if (!selected) return EVENT_PROCESSED;

				/* Special trick: if the folder is already
				 * folded, jump at the parent folder, so the
				 * next time when user presses the key, the
				 * whole parent folder will be closed. */
				if (list_empty(selected->child)
				    || !selected->expanded) {
					struct ctx ctx = { selected, 1 };

					if (!selected->root) break;

					traverse_listbox_items_list(
							selected->root, box, 0, 1,
							test_search, &ctx);
					box_sel_move(dlg_data->widgets_data,
						     ctx.offset);

				} else if (selected->type == BI_FOLDER) {
					recursively_set_expanded(selected, 0);
				}

				break;
			}

			/* Recursively expand all folders */
			if (ev->x == ']' || ev->x == '+' || ev->x == '=') {
				if (!selected || box->sel->type != BI_FOLDER)
					return EVENT_PROCESSED;

				recursively_set_expanded(box->sel, 1);
				break;
			}

			return EVENT_NOT_PROCESSED;
		}

		case EV_INIT:
		{
			struct hierbox_browser *browser = dlg_data->dlg->udata2;
			struct hierbox_dialog_list_item *item;
			struct listbox_item *litem;

			/* If we fail here it only means automatic updating
			 * will not be possible so no need to panic. */
			item = mem_alloc(sizeof(struct hierbox_dialog_list_item));
			if (item) {
				item->dlg_data = dlg_data;
				add_to_list(browser->dialogs, item);
			}

			foreach (litem, *browser->items) {
				litem->visible = 1;
			}
		}
		case EV_RESIZE:
		case EV_REDRAW:
		case EV_MOUSE:
			return EVENT_NOT_PROCESSED;

		case EV_ABORT:
		{
			struct listbox_data *box = get_dlg_listbox_data(dlg_data);
			struct hierbox_browser *browser = dlg_data->dlg->udata2;
			struct hierbox_dialog_list_item *item;

			/* Delete the box structure */
			del_from_list(box);

			/* Delete the dialog list entry */
			foreach (item, browser->dialogs) {
				if (item->dlg_data == dlg_data) {
					del_from_list(item);
					mem_free(item);
					break;
				}
			}
			return EVENT_NOT_PROCESSED;
		}
	}

	display_dlg_item(dlg_data, dlg_data->widgets_data, 1);

	return EVENT_PROCESSED;
}

struct dialog_data *
hierbox_browser(struct hierbox_browser *browser, struct session *ses)
{
	struct terminal *term = ses->tab->term;
	struct listbox_data *listbox_data;
	struct dialog *dlg;
	int button = browser->buttons_size + 2;

	assert(ses);

	dlg = calloc_dialog(button, sizeof(struct listbox_data));
	if (!dlg) return NULL;

	listbox_data = (struct listbox_data *) get_dialog_offset(dlg, button);

	dlg->title = _(browser->title, term);
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.maximize_width = 1;
	dlg->layout.padding_top = 1;
	dlg->handle_event = hierbox_dialog_event_handler;
	dlg->udata = ses;
	dlg->udata2 = browser;

	add_dlg_listbox(dlg, 12, listbox_data);

	for (button = 0; button < browser->buttons_size; button++) {
		hierbox_button_handler handler = browser->buttons[button].handler;
		unsigned char *label = browser->buttons[button].label;

		add_dlg_button(dlg, B_ENTER, handler, _(label, term), NULL);
	}

	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Close", term), NULL);
	add_dlg_end(dlg, button + 2);

	return do_dialog(term, dlg, getml(dlg, NULL));
}

/* Action info management */

struct hierbox_action_info {
	/* The item (and especially its ->udata) to delete. If NULL it means we
	 * should destroy marked items instead. */
	struct listbox_item *item;
	struct listbox_data *box;
	struct terminal *term;
};

static int
scan_for_marks(struct listbox_item *item, void *info_, int *offset)
{
	if (item->marked) {
		struct hierbox_action_info *action_info = info_;

		action_info->item = NULL;
		*offset = 0;
	}

	return 0;
}

static int
scan_for_used(struct listbox_item *item, void *info_, int *offset)
{
	struct hierbox_action_info *action_info = info_;

	if (action_info->box->ops->is_used(item)) {
		action_info->item = item;
		*offset = 0;
	}


	return 0;
}

static struct hierbox_action_info *
init_hierbox_action_info(struct listbox_data *box, struct terminal *term,
			 struct listbox_item *item,
			 int (*scanner)(struct listbox_item *, void *, int *))
{
	struct hierbox_action_info *action_info;

	action_info = mem_alloc(sizeof(struct hierbox_action_info));
	if (!action_info) return NULL;

	action_info->item = item;;
	action_info->term = term;
	action_info->box = box;

	if (!scanner) return action_info;

	/* Look if it wouldn't be more interesting to blast off the marked
	 * item. */
	assert(!list_empty(*box->items));
	traverse_listbox_items_list(box->items->next, box, 0, 0,
				    scanner, action_info);

	return action_info;
}

static void
done_hierbox_action_info(void *action_info_)
{
	struct hierbox_action_info *action_info = action_info_;

	if (action_info->item)
		action_info->box->ops->unlock(action_info->item);
}

/* Info action */

int
push_hierbox_info_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct hierbox_action_info *action_info;
	unsigned char *msg;

	if (!box->sel || !box->sel->udata) return 0;

	assert(box->ops);

	action_info = init_hierbox_action_info(box, term, box->sel, NULL);
	if (!action_info) return 0;

	msg = box->ops->get_info(action_info->item, term, LISTBOX_ALL);
	if (!msg) {
		mem_free(action_info);
		return 0;
	}

	box->ops->lock(action_info->item);

	msg_box(term, getml(action_info, NULL), MSGBOX_FREE_TEXT,
		N_("Info"), AL_LEFT,
		msg,
		action_info, 1,
		N_("OK"), done_hierbox_action_info, B_ESC | B_ENTER);

	return 0;
}

/* Goto action */

int
push_hierbox_goto_button(struct dialog_data *dlg_data,
			 struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct session *ses = dlg_data->dlg->udata;
	unsigned char *uri;

	/* Do nothing with a folder */
	/* TODO: Maybe pop up a msg_box() with an error message. --jonas */
	if (!box->sel || box->sel->type == BI_FOLDER) return 0;

	/* Follow the bookmark */
	uri = box->ops->get_info(box->sel, dlg_data->win->term, LISTBOX_URI);
	if (!uri) return 0;

	goto_url(ses, uri);
	mem_free(uri);

	/* Close the dialog */
	delete_window(dlg_data->win);
	return 0;
}

/* Delete action */

static void
print_delete_error(struct listbox_item *item, struct terminal *term,
		   struct listbox_ops *ops)
{
	if (item->type == BI_FOLDER) {
		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("Deleting used folder"), AL_CENTER,
			msg_text(term, N_("Sorry, but the folder \"%s\""
				 " is being used by something else."),
				 item->text),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);

	} else {
		unsigned char *msg = ops->get_info(item, term, LISTBOX_ALL);

		if (!msg) return;

		msg_box(term, getml(msg, NULL), MSGBOX_FREE_TEXT,
			N_("Deleting used item"), AL_CENTER,
			msg_text(term, N_("Sorry, but the item \"%s\""
				 " is being used by something else.\n\n"
				 "%s"),
				item->text, msg),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
	}
}

static void
do_delete_item(struct listbox_item *item, struct hierbox_action_info *info,
	       int last)
{
	struct listbox_ops *ops = info->box->ops;

	assert(item && item->udata);

	/* FIXME: Hmm maybe this silent treatment is not optimal. Ideally we
	 * should probably combine the delete error message to also be usable
	 * for this. --jonas */
	if (!ops->can_delete(item)) return;

	if (ops->is_used(item)) {
		print_delete_error(item, info->term, ops);
		return;
	}

	if (item->type == BI_FOLDER) {
		struct listbox_item *child = item->child.next;

		while (child != (void *) &item->child) {
			child = child->next;
			do_delete_item(child->prev, info, 0);
		}
	}

	if (list_empty(item->child))
		ops->delete(item, last);
}

static int
delete_marked(struct listbox_item *item, void *data_, int *offset)
{
	struct hierbox_action_info *delete_info = data_;

	if (item->marked && !delete_info->box->ops->is_used(item)) {
		/* Save the first marked so it can be deleted last */
		if (!delete_info->item) {
			delete_info->item = item;
		} else {
			do_delete_item(item, delete_info, 0);
		}

		return 1;
	}

	return 0;
}

static void
push_ok_delete_button(void *delete_info_)
{
	struct hierbox_action_info *delete_info = delete_info_;

	if (delete_info->item) {
		delete_info->box->ops->unlock(delete_info->item);
	} else {
		traverse_listbox_items_list(delete_info->box->items->next,
					    delete_info->box, 0, 0,
					    delete_marked, delete_info);
		if (!delete_info->item) return;
	}

	/* Delete the last one (traversal should save one to delete) */
	do_delete_item(delete_info->item, delete_info, 1);
}

int
push_hierbox_delete_button(struct dialog_data *dlg_data,
			   struct widget_data *button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct hierbox_action_info *delete_info;

	if (!box->sel || !box->sel->udata) return 0;

	assert(box->ops && box->ops->can_delete && box->ops->delete);

	delete_info = init_hierbox_action_info(box, term, box->sel, scan_for_marks);
	if (!delete_info) return 0;

	if (!delete_info->item) {
		msg_box(term, getml(delete_info, NULL), 0,
			N_("Delete marked items"), AL_CENTER,
			N_("Delete marked items?"),
			delete_info, 2,
			N_("Yes"), push_ok_delete_button, B_ENTER,
			N_("No"), done_hierbox_action_info, B_ESC);
		return 0;
	}

	if (!box->ops->can_delete(delete_info->item)) {
		msg_box(term, getml(delete_info, NULL), MSGBOX_FREE_TEXT,
			N_("Delete item"), AL_CENTER,
			msg_text(term, N_("Cannot delete \"%s\""),
				delete_info->item->text),
			NULL, 1,
			N_("OK"), NULL, B_ESC | B_ENTER);
		return 0;

	} else if (delete_info->item->type == BI_FOLDER) {
		box->ops->lock(delete_info->item);
		msg_box(term, getml(delete_info, NULL), MSGBOX_FREE_TEXT,
			N_("Delete folder"), AL_CENTER,
			msg_text(term, N_("Delete the folder \"%s\" and its content?"),
				 delete_info->item->text),
			delete_info, 2,
			N_("Yes"), push_ok_delete_button, B_ENTER,
			N_("No"), done_hierbox_action_info, B_ESC);
	} else {
		unsigned char *msg;

		msg = box->ops->get_info(delete_info->item, term, LISTBOX_ALL);
		box->ops->lock(delete_info->item);

		msg_box(term, getml(delete_info, msg, NULL), MSGBOX_FREE_TEXT,
			N_("Delete item"), AL_CENTER,
			msg_text(term, N_("Delete \"%s\"?\n\n"
				"%s"),
				delete_info->item->text, empty_string_or_(msg)),
			delete_info, 2,
			N_("Yes"), push_ok_delete_button, B_ENTER,
			N_("No"), done_hierbox_action_info, B_ESC);
	}

	return 0;
}

/* Clear action */

static int
delete_unused(struct listbox_item *item, void *data_, int *offset)
{
	struct hierbox_action_info *action_info = data_;

	if (action_info->box->ops->is_used(item)) return 0;

	do_delete_item(item, action_info, 0);
	return 1;
}

static void
do_clear_browser(void *action_info_)
{
	struct hierbox_action_info *action_info = action_info_;

	traverse_listbox_items_list(action_info->box->items->next,
				    action_info->box, 0, 0,
				    delete_unused, action_info);
}

int
push_hierbox_clear_button(struct dialog_data *dlg_data,
			  struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct hierbox_action_info *action_info;

	if (!box->sel || !box->sel->udata) return 0;

	assert(box->ops);

	action_info = init_hierbox_action_info(box, term, NULL, scan_for_used);
	if (!action_info) return 0;

	if (action_info->item) {
		print_delete_error(action_info->item, term, box->ops);
		mem_free(action_info);
		return 0;
	}

	msg_box(term, getml(action_info, NULL), 0,
		N_("Clear all items"), AL_CENTER,
		N_("Do you really want to remove all items?"),
		action_info, 2,
		N_("Yes"), do_clear_browser, B_ENTER,
		N_("No"), NULL, B_ESC);

	return 0;
}
