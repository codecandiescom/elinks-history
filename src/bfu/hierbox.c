/* Hiearchic listboxes browser dialog commons */
/* $Id: hierbox.c,v 1.34 2003/10/30 15:50:54 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "bfu/text.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "dialogs/hierbox.h"
#include "intl/gettext/libintl.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"

static void
recursively_set_expanded(struct listbox_item *box, int expanded)
{
	struct listbox_item *child;

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

int
hierbox_dialog_event_handler(struct dialog_data *dlg_data, struct term_event *ev)
{
	switch (ev->ev) {
		case EV_KBD:
		{
			int n = dlg_data->n - 1;
			struct listbox_data *box;

                        if (dlg_data->widgets_data[n].widget->ops->kbd
			    && dlg_data->widgets_data[n].widget->ops->kbd(&dlg_data->widgets_data[n], dlg_data, ev)
			       == EVENT_PROCESSED)
				return EVENT_PROCESSED;

			box = (struct listbox_data *) dlg_data->widgets_data[n].widget->data;

			if (ev->x == ' ') {
				if (box->sel) {
					box->sel->expanded = !box->sel->expanded;
					goto display_dlg;
				}
				return EVENT_PROCESSED;
			}

			if (ev->x == '[' || ev->x == '-' || ev->x == '_') {
				if (box->sel) {
					if (list_empty(box->sel->child)
					    || !box->sel->expanded) {
						if (box->sel->root) {
							struct ctx ctx =
								{ box->sel, 1 };

							traverse_listbox_items_list(
									box->sel
									 ->root,
									0, 0,
									test_search,
									&ctx);
							box_sel_move(
								&dlg_data->widgets_data[n],
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

			if (ev->x == ']' || ev->x == '+' || ev->x == '=') {
				if (box->sel) {
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
			display_dlg_item(dlg_data, &dlg_data->widgets_data[n], 1);

			return EVENT_PROCESSED;
		}
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


void
layout_hierbox_browser(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	int n = dlg_data->n - 1;

	/* Find dimensions of dialog */
	text_width(term, dlg_data->dlg->title, &min, &max);
#if 0
	buttons_width(dlg->widgets + 2, 2, &min, &max);
#endif
	buttons_width(dlg_data->widgets_data, n, &min, &max);

	w = term->width * 9 / 10 - 2 * DIALOG_LB;
	/* We ignore this now, as we don't compute with the width of the listbox
	 * itself and we want it to have the maximal width possible. */
	/* int_upper_bound(&w, max); */
	int_lower_bound(&w, min);
	int_bounds(&w, 1, term->width - 2 * DIALOG_LB);

	rw = w;

	y += 1;	/* Blankline between top and top of box */
	dlg_format_box(NULL, term, &dlg_data->widgets_data[n], dlg_data->x + DIALOG_LB,
		       &y, w, NULL, AL_LEFT);
	y += 1;	/* Blankline between box and menu */
	dlg_format_buttons(NULL, term, dlg_data->widgets_data, n, 0,
			   &y, w, &rw, AL_CENTER);
	w = rw;
	dlg_data->xw = w + 2 * DIALOG_LB;
	dlg_data->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg_data);
	draw_dlg(dlg_data);
	y = dlg_data->y + DIALOG_TB;

	y++;
	dlg_format_box(term, term, &dlg_data->widgets_data[n], dlg_data->x + DIALOG_LB,
		       &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg_data->widgets_data[0], n,
			   dlg_data->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}
