/* Hiearchic listboxes browser dialog commons */
/* $Id: hierbox.c,v 1.55 2003/11/09 14:02:56 pasky Exp $ */

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
			struct listbox_data *box;

			/* Check if listbox has something to say to this */
                        if (widget_data->widget->ops->kbd
			    && widget_data->widget->ops->kbd(widget_data,
				   			     dlg_data, ev)
			       == EVENT_PROCESSED)
				return EVENT_PROCESSED;

			box = get_dlg_listbox_data(dlg_data);

			if (ev->x == ' ') {
				if (box->sel) {
					if (box->sel->type != BI_FOLDER)
						return EVENT_NOT_PROCESSED;
					box->sel->expanded = !box->sel->expanded;
					goto display_dlg;
				}
				return EVENT_PROCESSED;
			}

			/* Recursively unexpand all folders */
			if (ev->x == '[' || ev->x == '-' || ev->x == '_') {
				if (box->sel && box->sel->type == BI_FOLDER) {
					/* Special trick: if the folder is
					 * already folded, jump at the parent
					 * folder, so the next time when user
					 * presses the key, the whole parent
					 * folder will be closed. */
					if (list_empty(box->sel->child)
					    || !box->sel->expanded) {
						if (box->sel->root) {
							struct ctx ctx =
								{ box->sel, 1 };

							traverse_listbox_items_list(
								box->sel->root,
								0, 0,
								test_search,
								&ctx);
							box_sel_move(
								dlg_data->widgets_data,
								ctx.offset);
						}
					} else {
						recursively_set_expanded(
								box->sel, 0);
					}
					goto display_dlg;
				}
				return EVENT_PROCESSED;
			}

			/* Recursively expand all folders */
			if (ev->x == ']' || ev->x == '+' || ev->x == '=') {
				if (box->sel && box->sel->type == BI_FOLDER) {
					recursively_set_expanded(box->sel, 1);
					goto display_dlg;
				}
				return EVENT_PROCESSED;
			}

			return EVENT_NOT_PROCESSED;

display_dlg:
#ifdef BOOKMARKS
			/* FIXME - move from here to bookmarks/dialogs.c! */
			bookmarks_dirty = 1;
#endif
			display_dlg_item(dlg_data, dlg_data->widgets_data, 1);

			return EVENT_PROCESSED;
		}

		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
		case EV_MOUSE:
			break;

		case EV_ABORT:
		{
			/* Clean up after the dialog */
			struct listbox_data *box = get_dlg_listbox_data(dlg_data);

			del_from_list(box);
			/* Delete the box structure */
			mem_free(box);
			break;
		}

		default:
			internal("Unknown event received: %d", ev->ev);
	}

	return EVENT_NOT_PROCESSED;
}


struct dialog_data *
hierbox_browser(struct terminal *term, unsigned char *title, size_t add_size,
		struct listbox_data *listbox_data, void *udata,
		size_t buttons, ...)
{
	struct dialog *dlg;
	va_list ap;

	if (!listbox_data) return NULL;

	/* Create the dialog */
	dlg = calloc_dialog(buttons + 2, add_size);
	if (!dlg) {
		mem_free(listbox_data);
		return NULL;
	}

	dlg->title = _(title, term);
	dlg->layouter = generic_dialog_layouter;
	dlg->align = AL_NONE;
	dlg->handle_event = hierbox_dialog_event_handler;
	dlg->udata = udata;

	add_dlg_listbox(dlg, 12, listbox_data);

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
