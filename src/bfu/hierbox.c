/* Hiearchic listboxes browser dialog commons */
/* $Id: hierbox.c,v 1.76 2003/11/22 01:27:04 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
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
init_browser_box(struct hierbox_browser *browser, unsigned char *text,
		 void *data)
{
	struct listbox_item *box = mem_calloc(1, sizeof(struct listbox_item));

	if (!box) return NULL;

	init_list(box->child);
	box->visible = 1;

	box->text = text;
	box->box = &browser->boxes;
	box->udata = data;

	add_to_list(*browser->items, box);

	update_hierbox_browser(browser);

	return box;
}

/* Find a box to replace @item. This is done by trying first to traverse down, then
 * up and if both traversals end up returning the box we want to replace bail
 * out using NULL. */
static inline struct listbox_item *
replace_box_item(struct listbox_item *item)
{
	struct listbox_item *box;

	box = traverse_listbox_items_list(item, 1, 1, NULL, NULL);
	if (item != box) return box;

	box = traverse_listbox_items_list(item, -1, 1, NULL, NULL);
	return (item == box) ? NULL : box;
}

void
done_browser_box(struct hierbox_browser *browser, struct listbox_item *box_item)
{
	struct listbox_data *box_data;

	assert(box_item);

	/* If we are removing the top or the selected box we have to figure out
	 * a replacement. */

	foreach (box_data, *box_item->box) {
		if (box_data->sel && box_data->sel == box_item)
			box_data->sel = replace_box_item(box_item);

		if (box_data->top && box_data->top == box_item)
			box_data->top = replace_box_item(box_item);
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
							selected->root, 0, 1,
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
			mem_free(box);

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
hierbox_browser(struct terminal *term, unsigned char *title, size_t add_size,
		struct hierbox_browser *browser, void *udata,
		size_t buttons, ...)
{
	struct dialog *dlg = calloc_dialog(buttons + 2, add_size);
	va_list ap;

	if (!dlg) return NULL;

	dlg->title = _(title, term);
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.maximize_width = 1;
	dlg->layout.padding_top = 1;
	dlg->handle_event = hierbox_dialog_event_handler;
	dlg->udata = udata;
	dlg->udata2 = browser;

	add_dlg_listbox(dlg, 12);

	va_start(ap, buttons);

	while (dlg->widgets_size < buttons + 1) {
		unsigned char *label;
		int (*handler)(struct dialog_data *, struct widget_data *);
		void *data;
		int key;

		label = va_arg(ap, unsigned char *);
		handler = va_arg(ap, void *);
		key = va_arg(ap, int);
		data = va_arg(ap, void *);

		if (!label) {
			/* Skip this button. */
			buttons--;
			continue;
		}

		add_dlg_button(dlg, key, handler, _(label, term), data);
	}

	va_end(ap);

	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Close", term), NULL);
	add_dlg_end(dlg, buttons + 2);

	return do_dialog(term, dlg, getml(dlg, NULL));
}
