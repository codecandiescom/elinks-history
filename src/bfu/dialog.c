/* Dialog box implementation. */
/* $Id: dialog.c,v 1.46 2003/10/25 11:29:58 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"
#include "setup.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/listbox.h"
#include "bfu/style.h"
#include "bfu/widget.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
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
void dialog_func(struct window *, struct term_event *, int);


struct dialog_data *
do_dialog(struct terminal *term, struct dialog *dlg,
	  struct memory_list *ml)
{
	struct dialog_data *dlg_data;
	struct widget *d;
	int n = 0;

	/* FIXME: maintain a counter, and don't recount each time. --Zas */
	for (d = dlg->items; d->type != D_END; d++) n++;

	dlg_data = mem_alloc(sizeof(struct dialog_data) +
		             sizeof(struct widget_data) * n);
	if (!dlg_data) return NULL;

	dlg_data->dlg = dlg;
	dlg_data->n = n;
	dlg_data->ml = ml;
	add_window(term, dialog_func, dlg_data);

	return dlg_data;
}

static void
redraw_dialog(struct dialog_data *dlg_data)
{
	int i;
	int x = dlg_data->x + DIALOG_LEFT_BORDER;
	int y = dlg_data->y + DIALOG_TOP_BORDER;
	struct terminal *term = dlg_data->win->term;
	struct color_pair *title_color;

	draw_border(term, x, y,
		    dlg_data->xw - 2 * DIALOG_LEFT_BORDER,
		    dlg_data->yw - 2 * DIALOG_TOP_BORDER,
		    get_bfu_color(term, "dialog.frame"),
		    DIALOG_FRAME);

	title_color = get_bfu_color(term, "dialog.title");
	if (title_color) {
		unsigned char *title = dlg_data->dlg->title;
		int titlelen = strlen(title);

		x = (dlg_data->xw - titlelen) / 2 + dlg_data->x;
		draw_text(term, x - 1, y, " ", 1, 0, title_color);
		draw_text(term, x, y, title, titlelen, 0, title_color);
		draw_text(term, x + titlelen, y, " ", 1, 0, title_color);
	}

	for (i = 0; i < dlg_data->n; i++)
		display_dlg_item(dlg_data, &dlg_data->items[i], i == dlg_data->selected);

	redraw_from_window(dlg_data->win);
}

static void
select_dlg_item(struct dialog_data *dlg_data, int i)
{
	if (dlg_data->selected != i) {
		display_dlg_item(dlg_data, &dlg_data->items[dlg_data->selected], 0);
		display_dlg_item(dlg_data, &dlg_data->items[i], 1);
		dlg_data->selected = i;
	}
	if (dlg_data->items[i].item->ops->select)
		dlg_data->items[i].item->ops->select(&dlg_data->items[i], dlg_data);
}


/* TODO: This is too long and ugly. Rewrite and split. */
void
dialog_func(struct window *win, struct term_event *ev, int fwd)
{
	int i;
	struct dialog_data *dlg_data = win->data;

	dlg_data->win = win;

	/* Look whether user event handlers can help us.. */
	if (dlg_data->dlg->handle_event &&
	    (dlg_data->dlg->handle_event(dlg_data, ev) == EVENT_PROCESSED)) {
		return;
	}

	switch (ev->ev) {
		case EV_INIT:
			for (i = 0; i < dlg_data->n; i++) {
				struct widget_data *widget = &dlg_data->items[i];

				memset(widget, 0, sizeof(struct widget_data));
				widget->item = &dlg_data->dlg->items[i];

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
					widget->item->ops->init(widget, dlg_data,
								ev);
			}
			dlg_data->selected = 0;

		case EV_RESIZE:
		case EV_REDRAW:
			dlg_data->dlg->fn(dlg_data);
			redraw_dialog(dlg_data);
			break;

		case EV_MOUSE:
#ifdef USE_MOUSE
			for (i = 0; i < dlg_data->n; i++)
				if (dlg_data->items[i].item->ops->mouse
				    && dlg_data->items[i].item->ops->mouse(&dlg_data->items[i], dlg_data, ev)
				       == EVENT_PROCESSED)
					break;
#endif /* USE_MOUSE */
			break;

		case EV_KBD:
			{
			struct widget_data *di;

			di = &dlg_data->items[dlg_data->selected];

			/* First let the widget try out. */
			if (di->item->ops->kbd
			    && di->item->ops->kbd(di, dlg_data, ev)
			       == EVENT_PROCESSED)
				break;

			/* Can we select? */
			if ((ev->x == KBD_ENTER || ev->x == ' ')
			    && di->item->ops->select) {
				di->item->ops->select(di, dlg_data);
				break;
			}

			/* Look up for a button with matching starting letter. */
			if (ev->x > ' ' && ev->x < 0x100) {
				for (i = 0; i < dlg_data->n; i++)
					if (dlg_data->dlg->items[i].type == D_BUTTON
					    && upcase(dlg_data->dlg->items[i].text[0])
					       == upcase(ev->x)) {
						select_dlg_item(dlg_data, i);
						return;
					}
			}

			/* Submit button. */
			if (ev->x == KBD_ENTER
			    && (di->item->type == D_FIELD
				|| di->item->type == D_FIELD_PASS
				|| ev->y == KBD_CTRL || ev->y == KBD_ALT)) {
				for (i = 0; i < dlg_data->n; i++)
					if (dlg_data->dlg->items[i].type == D_BUTTON
					    && dlg_data->dlg->items[i].gid & B_ENTER) {
						select_dlg_item(dlg_data, i);
						return;
					}
			}

			/* Cancel button. */
			if (ev->x == KBD_ESC) {
				for (i = 0; i < dlg_data->n; i++)
					if (dlg_data->dlg->items[i].type == D_BUTTON
					    && dlg_data->dlg->items[i].gid & B_ESC) {
						select_dlg_item(dlg_data, i);
						return;
					}
			}

			/* Cycle focus. */

			if ((ev->x == KBD_TAB && !ev->y) || ev->x == KBD_DOWN
			    || ev->x == KBD_RIGHT) {
				display_dlg_item(dlg_data, &dlg_data->items[dlg_data->selected], 0);

				dlg_data->selected++;
				if (dlg_data->selected >= dlg_data->n)
					dlg_data->selected = 0;

				display_dlg_item(dlg_data, &dlg_data->items[dlg_data->selected], 1);
				redraw_from_window(dlg_data->win);
				break;
			}

			if ((ev->x == KBD_TAB && ev->y) || ev->x == KBD_UP
			    || ev->x == KBD_LEFT) {
				display_dlg_item(dlg_data, &dlg_data->items[dlg_data->selected], 0);

				dlg_data->selected--;
				if (dlg_data->selected < 0)
					dlg_data->selected = dlg_data->n - 1;

				display_dlg_item(dlg_data, &dlg_data->items[dlg_data->selected], 1);
				redraw_from_window(dlg_data->win);
				break;
			}

			break;
			}

		case EV_ABORT:
			/* Moved this line up so that the dlg would have access
			   to its member vars before they get freed. */
			if (dlg_data->dlg->abort)
				dlg_data->dlg->abort(dlg_data);

			for (i = 0; i < dlg_data->n; i++) {
				struct widget_data *di = &dlg_data->items[i];

				if (di->cdata) mem_free(di->cdata);
				free_list(di->history);
			}

			freeml(dlg_data->ml);
	}
}

int
check_dialog(struct dialog_data *dlg_data)
{
	int i;

	for (i = 0; i < dlg_data->n; i++) {
		if (dlg_data->dlg->items[i].type != D_CHECKBOX &&
		    dlg_data->dlg->items[i].type != D_FIELD &&
		    dlg_data->dlg->items[i].type != D_FIELD_PASS)
			continue;

		if (dlg_data->dlg->items[i].fn &&
		    dlg_data->dlg->items[i].fn(dlg_data, &dlg_data->items[i])) {
			dlg_data->selected = i;
			redraw_dialog(dlg_data);
			return 1;
		}
	}

	return 0;
}

int
cancel_dialog(struct dialog_data *dlg_data, struct widget_data *di)
{
	delete_window(dlg_data->win);
	return 0;
}

int
update_dialog_data(struct dialog_data *dlg_data, struct widget_data *di)
{
	int i;

	for (i = 0; i < dlg_data->n; i++)
		memcpy(dlg_data->dlg->items[i].data,
		       dlg_data->items[i].cdata,
		       dlg_data->dlg->items[i].dlen);

	return 0;
}

int
ok_dialog(struct dialog_data *dlg_data, struct widget_data *di)
{
	void (*fn)(void *) = dlg_data->dlg->refresh;
	void *data = dlg_data->dlg->refresh_data;

	if (check_dialog(dlg_data)) return 1;

	update_dialog_data(dlg_data, di);

	if (fn) fn(data);
	return cancel_dialog(dlg_data, di);
}

int
clear_dialog(struct dialog_data *dlg_data, struct widget_data *di)
{
	int i;

	for (i = 0; i < dlg_data->n; i++) {
		if (dlg_data->dlg->items[i].type != D_FIELD &&
		    dlg_data->dlg->items[i].type != D_FIELD_PASS)
			continue;
		memset(dlg_data->items[i].cdata, 0, dlg_data->dlg->items[i].dlen);
		dlg_data->items[i].cpos = 0;
	}

	redraw_dialog(dlg_data);
	return 0;
}

void
center_dlg(struct dialog_data *dlg_data)
{
	dlg_data->x = (dlg_data->win->term->x - dlg_data->xw) / 2;
	dlg_data->y = (dlg_data->win->term->y - dlg_data->yw) / 2;
}

void
draw_dlg(struct dialog_data *dlg_data)
{
	draw_area(dlg_data->win->term, dlg_data->x, dlg_data->y, dlg_data->xw, dlg_data->yw, ' ', 0,
		  get_bfu_color(dlg_data->win->term, "dialog.generic"));

	if (get_opt_bool("ui.dialogs.shadows")) {
		/* Draw shadow */
		struct color_pair * shadow_color;

		shadow_color = get_bfu_color(dlg_data->win->term, "dialog.shadow");

		/* (horizontal) */
		draw_area(dlg_data->win->term, dlg_data->x + 2, dlg_data->y + dlg_data->yw,
			  dlg_data->xw - 2, 1, ' ', 0, shadow_color);

		/* (vertical) */
		draw_area(dlg_data->win->term, dlg_data->x + dlg_data->xw, dlg_data->y + 1,
			  2, dlg_data->yw, ' ', 0, shadow_color);
	}
}
