/* Dialog box implementation. */
/* $Id: dialog.c,v 1.34 2003/06/07 12:05:11 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/listbox.h"
#include "bfu/widget.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"

#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"


/* Prototypes */
void dialog_func(struct window *, struct event *, int);


struct dialog_data *
do_dialog(struct terminal *term, struct dialog *dlg,
	  struct memory_list *ml)
{
	struct dialog_data *dd;
	struct widget *d;
	int n = 0;

	/* FIXME: maintain a counter, and don't recount each time. --Zas */
	for (d = dlg->items; d->type != D_END; d++) n++;

	dd = mem_alloc(sizeof(struct dialog_data) +
		       sizeof(struct widget_data) * n);
	if (!dd) return NULL;

	dd->dlg = dlg;
	dd->n = n;
	dd->ml = ml;
	add_window(term, dialog_func, dd);

	return dd;
}

static void
redraw_dialog(struct dialog_data *dlg)
{
	int i;
	int x = dlg->x + DIALOG_LEFT_BORDER;
	int y = dlg->y + DIALOG_TOP_BORDER;
	struct terminal *term = dlg->win->term;
	int dialog_title_color = get_bfu_color(term, "dialog.title");

	draw_frame(term, x, y,
		   dlg->xw - 2 * DIALOG_LEFT_BORDER,
		   dlg->yw - 2 * DIALOG_TOP_BORDER,
		   get_bfu_color(term, "dialog.frame"), DIALOG_FRAME);

	i = strlen(dlg->dlg->title);
	x = (dlg->xw - i) / 2 + dlg->x;
	print_text(term, x - 1, y, 1, " ", dialog_title_color);
	print_text(term, x, y, i, dlg->dlg->title, dialog_title_color);
	print_text(term, x + i, y, 1, " ", dialog_title_color);

	for (i = 0; i < dlg->n; i++)
		display_dlg_item(dlg, &dlg->items[i], i == dlg->selected);

	redraw_from_window(dlg->win);
}

static void
select_dlg_item(struct dialog_data *dlg, int i)
{
	if (dlg->selected != i) {
		display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
		display_dlg_item(dlg, &dlg->items[i], 1);
		dlg->selected = i;
	}
	if (dlg->items[i].item->ops->select)
		dlg->items[i].item->ops->select(&dlg->items[i], dlg);
}


/* TODO: This is too long and ugly. Rewrite and split. */
void
dialog_func(struct window *win, struct event *ev, int fwd)
{
	int i;
	struct terminal *term = win->term;
	struct dialog_data *dlg = win->data;

	dlg->win = win;

	/* Look whether user event handlers can help us.. */
	if (dlg->dlg->handle_event &&
	    (dlg->dlg->handle_event(dlg, ev) == EVENT_PROCESSED)) {
		return;
	}

	switch (ev->ev) {
		case EV_INIT:
			for (i = 0; i < dlg->n; i++) {
				struct widget_data *widget = &dlg->items[i];

				memset(widget, 0, sizeof(struct widget_data));
				widget->item = &dlg->dlg->items[i];

				if (widget->item->dlen) {
					widget->cdata = mem_alloc(widget->item->dlen);
					if (widget->cdata) {
						memcpy(widget->cdata,
						       widget->item->data,
						       widget->item->dlen);
					} else {
						continue;
					}
				}
				/* XXX: REMOVE THIS! --pasky */
				/* XXX: YES YES ! GOOD IDEA ! --Zas */
				{
					struct widget_ops *w_o[] = {
						NULL,
						&checkbox_ops,
						&field_ops,
						&field_pass_ops,
						&button_ops,
						&listbox_ops,
					};

					widget->item->ops =
						w_o[widget->item->type];
				}

				init_list(widget->history);
				widget->cur_hist = (struct input_history_item *)
						   &widget->history;

				if (widget->item->ops->init)
					widget->item->ops->init(widget, dlg,
								ev);
			}
			dlg->selected = 0;

		case EV_RESIZE:
		case EV_REDRAW:
			dlg->dlg->fn(dlg);
			redraw_dialog(dlg);
			break;

		case EV_MOUSE:
			for (i = 0; i < dlg->n; i++)
				if (dlg->items[i].item->ops->mouse
				    && dlg->items[i].item->ops->mouse(&dlg->items[i], dlg, ev)
				       == EVENT_PROCESSED)
					break;
			break;

		case EV_KBD:
			{
			struct widget_data *di;

			di = &dlg->items[dlg->selected];

			/* First let the widget try out. */
			if (di->item->ops->kbd
			    && di->item->ops->kbd(di, dlg, ev)
			       == EVENT_PROCESSED)
				break;

			/* Can we select? */
			if ((ev->x == KBD_ENTER || ev->x == ' ')
			    && di->item->ops->select) {
				di->item->ops->select(di, dlg);
				break;
			}

			/* Look up for a button with matching starting letter. */
			if (ev->x > ' ' && ev->x < 0x100) {
				for (i = 0; i < dlg->n; i++)
					if (dlg->dlg->items[i].type == D_BUTTON
					    && upcase(dlg->dlg->items[i].text[0])
					       == upcase(ev->x)) {
						select_dlg_item(dlg, i);
						return;
					}
			}

			/* Submit button. */
			if (ev->x == KBD_ENTER
			    && (di->item->type == D_FIELD
				|| di->item->type == D_FIELD_PASS
			        || ev->y == KBD_CTRL || ev->y == KBD_ALT)) {
				for (i = 0; i < dlg->n; i++)
					if (dlg->dlg->items[i].type == D_BUTTON
					    && dlg->dlg->items[i].gid & B_ENTER) {
						select_dlg_item(dlg, i);
						return;
					}
			}

			/* Cancel button. */
			if (ev->x == KBD_ESC) {
				for (i = 0; i < dlg->n; i++)
					if (dlg->dlg->items[i].type == D_BUTTON
					    && dlg->dlg->items[i].gid & B_ESC) {
						select_dlg_item(dlg, i);
						return;
					}
			}

			/* Cycle focus. */

			if ((ev->x == KBD_TAB && !ev->y) || ev->x == KBD_DOWN
			    || ev->x == KBD_RIGHT) {
				display_dlg_item(dlg, &dlg->items[dlg->selected], 0);

				dlg->selected++;
				if (dlg->selected >= dlg->n)
					dlg->selected = 0;

				display_dlg_item(dlg, &dlg->items[dlg->selected], 1);
				redraw_from_window(dlg->win);
				break;
			}

			if ((ev->x == KBD_TAB && ev->y) || ev->x == KBD_UP
			    || ev->x == KBD_LEFT) {
				display_dlg_item(dlg, &dlg->items[dlg->selected], 0);

				dlg->selected--;
				if (dlg->selected < 0)
					dlg->selected = dlg->n - 1;

				display_dlg_item(dlg, &dlg->items[dlg->selected], 1);
				redraw_from_window(dlg->win);
				break;
			}

			break;
			}

		case EV_ABORT:
			/* Moved this line up so that the dlg would have access
			   to its member vars before they get freed. */
			if (dlg->dlg->abort)
				dlg->dlg->abort(dlg);

			for (i = 0; i < dlg->n; i++) {
				struct widget_data *di = &dlg->items[i];

				if (di->cdata) mem_free(di->cdata);
				free_list(di->history);
			}

			freeml(dlg->ml);
	}
}

int
check_dialog(struct dialog_data *dlg)
{
	int i;

	for (i = 0; i < dlg->n; i++) {
		if (dlg->dlg->items[i].type != D_CHECKBOX &&
		    dlg->dlg->items[i].type != D_FIELD &&
		    dlg->dlg->items[i].type != D_FIELD_PASS)
			continue;

		if (dlg->dlg->items[i].fn &&
		    dlg->dlg->items[i].fn(dlg, &dlg->items[i])) {
			dlg->selected = i;
			redraw_dialog(dlg);
			return 1;
		}
	}

	return 0;
}

int
cancel_dialog(struct dialog_data *dlg, struct widget_data *di)
{
	delete_window(dlg->win);
	return 0;
}

int
update_dialog_data(struct dialog_data *dlg, struct widget_data *di)
{
	int i;

	for (i = 0; i < dlg->n; i++)
		memcpy(dlg->dlg->items[i].data,
		       dlg->items[i].cdata,
		       dlg->dlg->items[i].dlen);

	return 0;
}

int
ok_dialog(struct dialog_data *dlg, struct widget_data *di)
{
	void (*fn)(void *) = dlg->dlg->refresh;
	void *data = dlg->dlg->refresh_data;

	if (check_dialog(dlg)) return 1;

	update_dialog_data(dlg, di);

	if (fn) fn(data);
	return cancel_dialog(dlg, di);
}

int
clear_dialog(struct dialog_data *dlg, struct widget_data *di)
{
	int i;

	for (i = 0; i < dlg->n; i++) {
		if (dlg->dlg->items[i].type != D_FIELD &&
		    dlg->dlg->items[i].type != D_FIELD_PASS)
			continue;
		memset(dlg->items[i].cdata, 0, dlg->dlg->items[i].dlen);
		dlg->items[i].cpos = 0;
	}

	redraw_dialog(dlg);
	return 0;
}

void
center_dlg(struct dialog_data *dlg)
{
	dlg->x = (dlg->win->term->x - dlg->xw) / 2;
	dlg->y = (dlg->win->term->y - dlg->yw) / 2;
}

void
draw_dlg(struct dialog_data *dlg)
{
	fill_area(dlg->win->term, dlg->x, dlg->y, dlg->xw, dlg->yw,
		  get_bfu_color(dlg->win->term, "=dialog"));

	if (get_opt_bool("ui.dialogs.shadows")) {
		/* Draw shadow */
		int shadow_color = get_bfu_color(dlg->win->term, "=dialog.shadow");

		/* (horizontal) */
		fill_area(dlg->win->term, dlg->x + 2, dlg->y + dlg->yw,
			  dlg->xw - 2, 1,
			  shadow_color);

		/* (vertical) */
		fill_area(dlg->win->term, dlg->x + dlg->xw, dlg->y + 1,
			  2, dlg->yw,
			  shadow_color);
	}
}
