/* Hiearchic listboxes browser dialog commons */
/* $Id: hierbox.c,v 1.13 2003/01/03 02:23:54 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/listbox.h"
#include "bfu/text.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "intl/gettext/libintl.h"


int
hierbox_dialog_event_handler(struct dialog_data *dlg, struct event *ev)
{
	switch (ev->ev) {
		case EV_KBD:
                        if (dlg->items[dlg->n - 1].item->ops->kbd
			    && dlg->items[dlg->n - 1].item->ops->kbd(&dlg->items[dlg->n - 1], dlg, ev)
			       == EVENT_PROCESSED)
				return EVENT_PROCESSED;

			if (ev->x == ' ') {
				struct listbox_data *box;

				box = (struct listbox_data *) dlg->items[dlg->n - 1].item->data;
				if (box->sel) {
					box->sel->expanded = !box->sel->expanded;
#ifdef BOOKMARKS
					/* FIXME - move from here to bookmarks/dialogs.c! */
					bookmarks_dirty = 1;
#endif
				}
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);

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
layout_hierbox_browser(struct dialog_data *dlg)
{
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct terminal *term;

	term = dlg->win->term;

	/* Find dimensions of dialog */
	max_text_width(term, dlg->dlg->title, &max);
	min_text_width(term, dlg->dlg->title, &min);
#if 0
	max_buttons_width(term, dlg->items + 2, 2, &max);
	min_buttons_width(term, dlg->items + 2, 2, &min);
#endif
	max_buttons_width(term, dlg->items, dlg->n - 1, &max);
	min_buttons_width(term, dlg->items, dlg->n - 1, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	/* We ignore this now, as we don't compute with the width of the listbox
	 * itself and we want it to have the maximal width possible. */
	/* if (w > max) w = max; */
	if (w < min) w = min;

	if (w > term->x - 2 * DIALOG_LB)
		w = term->x - 2 * DIALOG_LB;

	if (w < 1)
		w = 1;

	rw = w;

	y += 1;	/* Blankline between top and top of box */
	dlg_format_box(NULL, term, &dlg->items[dlg->n - 1], dlg->x + DIALOG_LB,
		       &y, w, NULL, AL_LEFT);
	y += 1;	/* Blankline between box and menu */
	dlg_format_buttons(NULL, term, dlg->items, dlg->n - 1, 0,
			   &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;

	y++;
	dlg_format_box(term, term, &dlg->items[dlg->n - 1], dlg->x + DIALOG_LB,
		       &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg->items[0], dlg->n - 1,
			   dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}
