/* Hiearchic listboxes browser dialog commons */
/* $Id: hierbox.c,v 1.169 2004/06/21 14:03:07 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "bookmarks/bookmarks.h"
#include "config/kbdbind.h"
#include "dialogs/download.h"
#include "intl/gettext/libintl.h"
#include "protocol/uri.h"
#include "sched/task.h"
#include "terminal/tab.h"
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


/* Common backend for listbox adding */
struct listbox_item *
add_listbox_item(struct hierbox_browser *browser, struct listbox_item *root,
		 enum listbox_item_type type, void *data, int add_position)
{
	struct listbox_item *item;

	if (!root) {
		assertm(browser, "Nowhere to add new list box item");
		root = &browser->root;
	}

	item = mem_calloc(1, sizeof(struct listbox_item));
	if (!item) return NULL;

	init_list(item->child);
	item->visible = 1;

	item->udata = data;
	item->type = type;
	item->depth = root->depth + 1;
	if (item->depth > 0) item->root = root;

	/* TODO: Possibility to sort by making add_position into a flag */
	if (add_position < 0)
		add_to_list_end(root->child, item);
	else
		add_to_list(root->child, item);

	if (browser) update_hierbox_browser(browser);

	return item;
}


/* Find a listbox item to replace @item. This is done by trying first to
 * traverse down then up, and if both traversals end up returning the @item
 * (that is, it is the last item in the box), return NULL. */
static inline struct listbox_item *
replace_listbox_item(struct listbox_item *item, struct listbox_data *data)
{
	struct listbox_item *box;

	box = traverse_listbox_items_list(item, data, 1, 1, NULL, NULL);
	if (item != box) return box;

	box = traverse_listbox_items_list(item, data, -1, 1, NULL, NULL);
	return (item == box) ? NULL : box;
}

void
done_listbox_item(struct hierbox_browser *browser, struct listbox_item *box_item)
{
	struct listbox_data *box_data;

	assert(box_item);

	/* If we are removing the top or the selected box we have to figure out
	 * a replacement. */

	foreach (box_data, browser->boxes) {
		if (box_data->sel == box_item)
			box_data->sel = replace_listbox_item(box_item, box_data);

		if (box_data->top == box_item)
			box_data->top = replace_listbox_item(box_item, box_data);
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

static int
test_search(struct listbox_item *item, void *data_, int *offset)
{
	struct listbox_context *listbox_context = data_;

	listbox_context->offset--;

	if (item == listbox_context->box->sel) *offset = 0;
	return 0;
}

static int
hierbox_ev_kbd(struct dialog_data *dlg_data, struct term_event *ev)
{
	struct widget_data *widget_data = dlg_data->widgets_data;
	struct widget *widget = widget_data->widget;
	struct listbox_data *box;
	struct listbox_item *selected;
	enum menu_action action;

	/* Check if listbox has something to say to this */
	if (widget->ops->kbd
	    && widget->ops->kbd(widget_data, dlg_data, ev)
	       == EVENT_PROCESSED)
		return EVENT_PROCESSED;

	box = get_dlg_listbox_data(dlg_data);
	selected = box->sel;
	action = kbd_action(KM_MENU, ev, NULL);

	if (action == ACT_MENU_SELECT) {
		if (!selected) return EVENT_PROCESSED;
		if (selected->type != BI_FOLDER)
			return EVENT_NOT_PROCESSED;
		selected->expanded = !selected->expanded;

	} else if (action == ACT_MENU_UNEXPAND) {
		/* Recursively unexpand all folders */
		if (!selected) return EVENT_PROCESSED;

		/* Special trick: if the folder is already
		 * folded, jump at the parent folder, so the
		 * next time when user presses the key, the
		 * whole parent folder will be closed. */
		if (list_empty(selected->child)
		    || !selected->expanded) {
			if (selected->root) {
				struct listbox_context ctx;

				memset(&ctx, 0, sizeof(struct listbox_context));
				ctx.box = box;
				ctx.offset = 1;

				traverse_listbox_items_list(
						selected->root, box, 0, 1,
						test_search, &ctx);
				box_sel_move(dlg_data->widgets_data,
					     ctx.offset);
			}

		} else if (selected->type == BI_FOLDER) {
			recursively_set_expanded(selected, 0);
		}

	} else if (action == ACT_MENU_EXPAND) {
		/* Recursively expand all folders */

		if (!selected || box->sel->type != BI_FOLDER)
			return EVENT_PROCESSED;

		recursively_set_expanded(box->sel, 1);

	} else {
		return EVENT_NOT_PROCESSED;

	}

#ifdef CONFIG_BOOKMARKS
	/* FIXME - move from here to bookmarks/dialogs.c! */
	/* We should probably call provided callback function, notifying the
	 * user of expansion change. --pasky */
	bookmarks_dirty = 1;
#endif

	display_dlg_item(dlg_data, dlg_data->widgets_data, 1);

	return EVENT_PROCESSED;
}

static int
hierbox_ev_init(struct dialog_data *dlg_data, struct term_event *ev)
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

	foreach (litem, browser->root.child) {
		litem->visible = 1;
	}

	return EVENT_NOT_PROCESSED;
}

static int
hierbox_ev_abort(struct dialog_data *dlg_data, struct term_event *ev)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct hierbox_browser *browser = dlg_data->dlg->udata2;
	struct hierbox_dialog_list_item *item;

	/* Save state and delete the box structure */
	/* FIXME: It's workaround for bug 397. Real fix is needed. */
	if (browser != &download_browser)
		memcpy(&browser->box_data, box, sizeof(struct listbox_data));
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
			return hierbox_ev_kbd(dlg_data, ev);

		case EV_INIT:
			return hierbox_ev_init(dlg_data, ev);

		case EV_RESIZE:
		case EV_REDRAW:
		case EV_MOUSE:
			return EVENT_NOT_PROCESSED;

		case EV_ABORT:
			return hierbox_ev_abort(dlg_data, ev);
	}

	return EVENT_NOT_PROCESSED;
}


struct dialog_data *
hierbox_browser(struct hierbox_browser *browser, struct session *ses)
{
	struct terminal *term = ses->tab->term;
	struct listbox_data *listbox_data;
	struct dialog *dlg;
	int button = browser->buttons_size + 2;
	int anonymous = get_cmd_opt_bool("anonymous");

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
		struct hierbox_browser_button *but = &browser->buttons[button];

		/* Skip buttons that should not be displayed in anonymous mode */
		if (anonymous && !but->anonymous) {
			anonymous++;
			continue;
		}

		add_dlg_button(dlg, B_ENTER, but->handler, _(but->label, term), NULL);
	}

	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Close", term), NULL);

	/* @anonymous was initially 1 if we are running in anonymous mode so we
	 * have to subtract one. */
	add_dlg_end(dlg, button + 2 - (anonymous ? anonymous - 1 : 0));

	return do_dialog(term, dlg, getml(dlg, NULL));
}


/* Action info management */

static int
scan_for_marks(struct listbox_item *item, void *info_, int *offset)
{
	if (item->marked) {
		struct listbox_context *context = info_;

		context->item = NULL;
		*offset = 0;
	}

	return 0;
}

static int
scan_for_used(struct listbox_item *item, void *info_, int *offset)
{
	struct listbox_context *context = info_;

	if (context->box->ops->is_used(item)) {
		context->item = item;
		*offset = 0;
	}

	return 0;
}


static struct listbox_context *
init_listbox_context(struct listbox_data *box, struct terminal *term,
		     struct listbox_item *item,
		     int (*scanner)(struct listbox_item *, void *, int *))
{
	struct listbox_context *context;

	context = mem_calloc(1, sizeof(struct listbox_context));
	if (!context) return NULL;

	context->item = item;;
	context->term = term;
	context->box = box;

	if (!scanner) return context;

	/* Look if it wouldn't be more interesting to blast off the marked
	 * item. */
	assert(!list_empty(*box->items));
	traverse_listbox_items_list(box->items->next, box, 0, 0,
				    scanner, context);

	return context;
}

static void
done_listbox_context(void *context_)
{
	struct listbox_context *context = context_;

	if (context->item)
		context->box->ops->unlock(context->item);
}


/* Info action */

int
push_hierbox_info_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct listbox_context *context;
	unsigned char *msg;

	if (!box->sel) return 0;

	if (box->sel->type == BI_FOLDER) {
		msg_box(term, NULL, 0,
		N_("Info"), AL_CENTER,
		_("Press space to expand this folder.", term),
		NULL, 1,
		N_("OK"), NULL, B_ESC | B_ENTER);

		return 0;
	}

	assert(box->ops);

	context = init_listbox_context(box, term, box->sel, NULL);
	if (!context) return 0;

	msg = box->ops->get_info(context->item, term, LISTBOX_ALL);
	if (!msg) {
		mem_free(context);
		return 0;
	}

	box->ops->lock(context->item);

	msg_box(term, getml(context, NULL), MSGBOX_FREE_TEXT /* | MSGBOX_SCROLLABLE */,
		N_("Info"), AL_LEFT,
		msg,
		context, 1,
		N_("OK"), done_listbox_context, B_ESC | B_ENTER);

	return 0;
}


/* Goto action */

static void
recursively_goto_listbox(struct session *ses, struct listbox_item *root,
			 struct listbox_data *box)
{
	struct listbox_item *item;

	foreach (item, root->child) {
		struct uri *uri;

		if (item->type == BI_FOLDER) {
			recursively_goto_listbox(ses, item, box);
			continue;
		}

		uri = box->ops->get_uri(item);
		if (!uri) continue;

		open_uri_in_new_tab(ses, uri, 1);
		done_uri(uri);
	}
}

static int
goto_marked(struct listbox_item *item, void *data_, int *offset)
{
	struct listbox_context *context = data_;

	if (item->marked) {
		struct session *ses = context->dlg_data->dlg->udata;
		struct listbox_data *box = context->box;
		struct uri *uri;

		if (item->type == BI_FOLDER) {
			recursively_goto_listbox(ses, item, box);
			return 0;
		}

		uri = box->ops->get_uri(item);
		if (!uri) return 0;

		open_uri_in_new_tab(ses, uri, 1);
		done_uri(uri);
	}

	return 0;
}

int
push_hierbox_goto_button(struct dialog_data *dlg_data,
			 struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct session *ses = dlg_data->dlg->udata;
	struct terminal *term = dlg_data->win->term;
	struct listbox_context *context;

	/* Do nothing with a folder */
	if (!box->sel) return 0;

	context = init_listbox_context(box, term, box->sel, scan_for_marks);
	if (!context) return 0;

	if (!context->item) {
		context->dlg_data = dlg_data;
		traverse_listbox_items_list(context->box->items->next,
					    context->box, 0, 0,
					    goto_marked, context);

	} else if (box->sel->type == BI_FOLDER) {
		recursively_goto_listbox(ses, box->sel, box);

	} else {
		struct uri *uri = box->ops->get_uri(box->sel);

		if (uri) {
			goto_uri(ses, uri);
			done_uri(uri);
		}
	}

	mem_free(context);

	/* Close the dialog */
	delete_window(dlg_data->win);
	return 0;
}


/* Delete action */

enum delete_error {
	DELETE_IMPOSSIBLE = 0,
	DELETE_LOCKED,
	DELETE_ERRORS,
};

unsigned char *delete_messages[2][DELETE_ERRORS] = {
	{
		N_("Sorry, but the item \"%s\" cannot be deleted."),
		N_("Sorry, but the item \"%s\" is being used by something else."),
	},
	{
		N_("Sorry, but the folder \"%s\" cannot be deleted."),
		N_("Sorry, but the folder \"%s\" is being used by something else."),
	},
};

static void
print_delete_error(struct listbox_item *item, struct terminal *term,
		   struct listbox_ops *ops, enum delete_error err)
{
	struct string msg;
	unsigned char *errmsg = delete_messages[(item->type == BI_FOLDER)][err];
	unsigned char *text = ops->get_info(item, term, LISTBOX_TEXT);

	if (!text || !init_string(&msg)) {
		mem_free_if(text);
		return;
	}

	add_format_to_string(&msg, _(errmsg, term), text);
	mem_free(text);

	if (item->type == BI_LEAF) {
		unsigned char *info = ops->get_info(item, term, LISTBOX_ALL);

		if (info) {
			add_format_to_string(&msg, "\n\n%s", info);
			mem_free(info);
		}
	}

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("Delete error"), AL_LEFT,
		msg.source,
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

static void
do_delete_item(struct listbox_item *item, struct listbox_context *info,
	       int last, int delete)
{
	struct listbox_ops *ops = info->box->ops;

	assert(item);

	if ((!delete && !ops->can_delete(item)) || ops->is_used(item)) {
		print_delete_error(item, info->term, ops,
				   delete ? DELETE_LOCKED : DELETE_IMPOSSIBLE);
		return;
	}

	if (item->type == BI_FOLDER) {
		struct listbox_item *child = item->child.next;

		while (child != (void *) &item->child) {
			child = child->next;
			/* Propagate the ``delete'' property down to children
			 * since if a parent can be deleted the child should
			 * just delete itself too. */
			do_delete_item(child->prev, info, 0, 1);
		}
	}

	if (list_empty(item->child))
		ops->delete(item, last);
}

static int
delete_marked(struct listbox_item *item, void *data_, int *offset)
{
	struct listbox_context *context = data_;

	if (item->marked && !context->box->ops->is_used(item)) {
		/* Save the first marked so it can be deleted last */
		if (!context->item) {
			context->item = item;
		} else {
			do_delete_item(item, context, 0, 0);
		}

		return 1;
	}

	return 0;
}

static void
push_ok_delete_button(void *context_)
{
	struct listbox_context *context = context_;
	int last = 0;

	if (context->item) {
		context->box->ops->unlock(context->item);
	} else {
		traverse_listbox_items_list(context->box->items->next,
					    context->box, 0, 0,
					    delete_marked, context);
		if (!context->item) return;
	}

	if (context->item->root) {
		last = context->item == context->item->root->child.prev;
	}

	/* Delete the last one (traversal should save one to delete) */
	do_delete_item(context->item, context, 1, 0);

	/* If removing the last item in a folder move focus to previous item in
	 * the folder or the root. */
	if (last) box_sel_move(context->widget_data, -1);
}

int
push_hierbox_delete_button(struct dialog_data *dlg_data,
			   struct widget_data *button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct listbox_context *context;
	unsigned char *text;
	enum delete_error delete;

	if (!box->sel) return 0;

	assert(box->ops && box->ops->can_delete && box->ops->delete);

	context = init_listbox_context(box, term, box->sel, scan_for_marks);
	if (!context) return 0;

	context->widget_data = dlg_data->widgets_data;

	if (!context->item) {
		msg_box(term, getml(context, NULL), 0,
			N_("Delete marked items"), AL_CENTER,
			N_("Delete marked items?"),
			context, 2,
			N_("Yes"), push_ok_delete_button, B_ENTER,
			N_("No"), done_listbox_context, B_ESC);
		return 0;
	}

	delete = box->ops->can_delete(context->item)
		 ? DELETE_LOCKED : DELETE_IMPOSSIBLE;

	if (delete == DELETE_IMPOSSIBLE || box->ops->is_used(context->item)) {
		print_delete_error(context->item, term, box->ops, delete);
		mem_free(context);
		return 0;
	}

	text = box->ops->get_info(context->item, term, LISTBOX_TEXT);
	if (!text) {
		mem_free(context);
		return 0;
	}

	if (context->item->type == BI_FOLDER) {
		box->ops->lock(context->item);
		msg_box(term, getml(context, NULL), MSGBOX_FREE_TEXT,
			N_("Delete folder"), AL_CENTER,
			msg_text(term, N_("Delete the folder \"%s\" and its content?"),
				 text),
			context, 2,
			N_("Yes"), push_ok_delete_button, B_ENTER,
			N_("No"), done_listbox_context, B_ESC);
	} else {
		unsigned char *msg;

		msg = box->ops->get_info(context->item, term, LISTBOX_ALL);
		box->ops->lock(context->item);

		msg_box(term, getml(context, NULL), MSGBOX_FREE_TEXT,
			N_("Delete item"), AL_LEFT,
			msg_text(term, N_("Delete \"%s\"?\n\n"
				"%s"),
				text, empty_string_or_(msg)),
			context, 2,
			N_("Yes"), push_ok_delete_button, B_ENTER,
			N_("No"), done_listbox_context, B_ESC);
		mem_free_if(msg);
	}
	mem_free(text);

	return 0;
}


/* Clear action */

static int
delete_unused(struct listbox_item *item, void *data_, int *offset)
{
	struct listbox_context *context = data_;

	if (context->box->ops->is_used(item)) return 0;

	do_delete_item(item, context, 0, 0);
	return 1;
}

static void
do_clear_browser(void *context_)
{
	struct listbox_context *context = context_;

	traverse_listbox_items_list(context->box->items->next,
				    context->box, 0, 0,
				    delete_unused, context);
}

int
push_hierbox_clear_button(struct dialog_data *dlg_data,
			  struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct listbox_context *context;

	if (!box->sel) return 0;

	assert(box->ops);

	context = init_listbox_context(box, term, NULL, scan_for_used);
	if (!context) return 0;

	if (context->item) {
		/* FIXME: If the clear button should be used for browsers where
		 * not all items can be deleted scan_for_used() should also can
		 * for undeletable and we should be able to pass either delete
		 * error types. */
		print_delete_error(context->item, term, box->ops, DELETE_LOCKED);
		mem_free(context);
		return 0;
	}

	msg_box(term, getml(context, NULL), 0,
		N_("Clear all items"), AL_CENTER,
		N_("Do you really want to remove all items?"),
		context, 2,
		N_("Yes"), do_clear_browser, B_ENTER,
		N_("No"), NULL, B_ESC);

	return 0;
}
