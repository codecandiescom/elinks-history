/* Hiearchic listboxes browser dialog commons */
/* $Id: hierbox.c,v 1.25 2003/09/01 13:00:58 zas Exp $ */

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


int
hierbox_dialog_event_handler(struct dialog_data *dlg, struct event *ev)
{
	switch (ev->ev) {
		case EV_KBD:
		{
			int n = dlg->n - 1;

                        if (dlg->items[n].item->ops->kbd
			    && dlg->items[n].item->ops->kbd(&dlg->items[n], dlg, ev)
			       == EVENT_PROCESSED)
				return EVENT_PROCESSED;

			if (ev->x == ' ') {
				struct listbox_data *box;

				box = (struct listbox_data *) dlg->items[n].item->data;
				if (box->sel) {
					box->sel->expanded = !box->sel->expanded;
#ifdef BOOKMARKS
					/* FIXME - move from here to bookmarks/dialogs.c! */
					bookmarks_dirty = 1;
#endif
				}
				display_dlg_item(dlg, &dlg->items[n], 1);

				return EVENT_PROCESSED;
			}
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
layout_hierbox_browser(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	int n = dlg->n - 1;

	/* Find dimensions of dialog */
	text_width(term, dlg->dlg->title, &min, &max);
#if 0
	buttons_width(term, dlg->items + 2, 2, &min, &max);
#endif
	buttons_width(term, dlg->items, n, &min, &max);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	/* We ignore this now, as we don't compute with the width of the listbox
	 * itself and we want it to have the maximal width possible. */
	/* int_upper_bound(&w, max); */
	int_lower_bound(&w, min);
	int_bounds(&w, 1, term->x - 2 * DIALOG_LB);

	rw = w;

	y += 1;	/* Blankline between top and top of box */
	dlg_format_box(NULL, term, &dlg->items[n], dlg->x + DIALOG_LB,
		       &y, w, NULL, AL_LEFT);
	y += 1;	/* Blankline between box and menu */
	dlg_format_buttons(NULL, term, dlg->items, n, 0,
			   &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;

	y++;
	dlg_format_box(term, term, &dlg->items[n], dlg->x + DIALOG_LB,
		       &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg->items[0], n,
			   dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}
