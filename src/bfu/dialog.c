/* Dialog box implementation. */
/* $Id: dialog.c,v 1.62 2003/10/29 14:56:27 zas Exp $ */

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
	struct widget *widget;
	int n = 0;

	/* FIXME: maintain a counter, and don't recount each time. --Zas */
	for (widget = dlg->widgets; widget->type != WIDGET_END; widget++) n++;

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
		display_dlg_item(dlg_data, &dlg_data->widgets_data[i], i == dlg_data->selected);

	redraw_from_window(dlg_data->win);
}

static void
select_dlg_item(struct dialog_data *dlg_data, int i)
{
	if (dlg_data->selected != i) {
		display_dlg_item(dlg_data, selected_widget(dlg_data), 0);
		display_dlg_item(dlg_data, &dlg_data->widgets_data[i], 1);
		dlg_data->selected = i;
	}
	if (dlg_data->widgets_data[i].widget->ops->select)
		dlg_data->widgets_data[i].widget->ops->select(&dlg_data->widgets_data[i], dlg_data);
}

static struct widget_ops *widget_type_to_ops[] = {
	NULL,
	&checkbox_ops,
	&field_ops,
	&field_pass_ops,
	&button_ops,
	&listbox_ops,
};

static inline struct widget_data *
init_widget(struct dialog_data *dlg_data, struct term_event *ev, int i)
{
	struct widget_data *widget_data = &dlg_data->widgets_data[i];

	memset(widget_data, 0, sizeof(struct widget_data));
	widget_data->widget = &dlg_data->dlg->widgets[i];

	if (widget_data->widget->datalen) {
		widget_data->cdata = mem_alloc(widget_data->widget->datalen);
		if (widget_data->cdata) {
			memcpy(widget_data->cdata,
			       widget_data->widget->data,
			       widget_data->widget->datalen);
		} else {
			return NULL;
		}
	}

	widget_data->widget->ops = widget_type_to_ops[widget_data->widget->type];

	if (widget_has_history(widget_data)) {
		init_list(widget_data->info.field.history);
		widget_data->info.field.cur_hist =
			(struct input_history_entry *) &widget_data->info.field.history;
	}

	if (widget_data->widget->ops->init)
		widget_data->widget->ops->init(widget_data, dlg_data, ev);

	return widget_data;
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
			for (i = 0; i < dlg_data->n; i++)
				init_widget(dlg_data, ev, i);
			dlg_data->selected = 0;

		case EV_RESIZE:
		case EV_REDRAW:
			dlg_data->dlg->fn(dlg_data);
			redraw_dialog(dlg_data);
			break;

		case EV_MOUSE:
#ifdef USE_MOUSE
			for (i = 0; i < dlg_data->n; i++)
				if (dlg_data->widgets_data[i].widget->ops->mouse
				    && dlg_data->widgets_data[i].widget->ops->mouse(&dlg_data->widgets_data[i], dlg_data, ev)
				       == EVENT_PROCESSED)
					break;
#endif /* USE_MOUSE */
			break;

		case EV_KBD:
			{
			struct widget_data *widget_data = selected_widget(dlg_data);

			/* First let the widget try out. */
			if (widget_data->widget->ops->kbd
			    && widget_data->widget->ops->kbd(widget_data, dlg_data, ev)
			       == EVENT_PROCESSED)
				break;

			/* Can we select? */
			if ((ev->x == KBD_ENTER || ev->x == ' ')
			    && widget_data->widget->ops->select) {
				widget_data->widget->ops->select(widget_data, dlg_data);
				break;
			}

			/* Look up for a button with matching starting letter. */
			if (ev->x > ' ' && ev->x < 0x100) {
				for (i = 0; i < dlg_data->n; i++)
					if (dlg_data->dlg->widgets[i].type == WIDGET_BUTTON
					    && upcase(dlg_data->dlg->widgets[i].text[0])
					       == upcase(ev->x)) {
						select_dlg_item(dlg_data, i);
						return;
					}
			}

			/* Submit button. */
			if (ev->x == KBD_ENTER
			    && (widget_is_textfield(widget_data)
				|| ev->y == KBD_CTRL || ev->y == KBD_ALT)) {
				for (i = 0; i < dlg_data->n; i++)
					if (dlg_data->dlg->widgets[i].type == WIDGET_BUTTON
					    && dlg_data->dlg->widgets[i].info.button.flags & B_ENTER) {
						select_dlg_item(dlg_data, i);
						return;
					}
			}

			/* Cancel button. */
			if (ev->x == KBD_ESC) {
				for (i = 0; i < dlg_data->n; i++)
					if (dlg_data->dlg->widgets[i].type == WIDGET_BUTTON
					    && dlg_data->dlg->widgets[i].info.button.flags & B_ESC) {
						select_dlg_item(dlg_data, i);
						return;
					}
			}

			/* Cycle focus. */

			if ((ev->x == KBD_TAB && !ev->y) || ev->x == KBD_DOWN
			    || ev->x == KBD_RIGHT) {
				display_dlg_item(dlg_data, selected_widget(dlg_data), 0);

				dlg_data->selected++;
				if (dlg_data->selected >= dlg_data->n)
					dlg_data->selected = 0;

				display_dlg_item(dlg_data, selected_widget(dlg_data), 1);
				redraw_from_window(dlg_data->win);
				break;
			}

			if ((ev->x == KBD_TAB && ev->y) || ev->x == KBD_UP
			    || ev->x == KBD_LEFT) {
				display_dlg_item(dlg_data, selected_widget(dlg_data), 0);

				dlg_data->selected--;
				if (dlg_data->selected < 0)
					dlg_data->selected = dlg_data->n - 1;

				display_dlg_item(dlg_data, selected_widget(dlg_data), 1);
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
				struct widget_data *widget_data = &dlg_data->widgets_data[i];

				if (widget_data->cdata) mem_free(widget_data->cdata);
				if (widget_has_history(widget_data))
					free_list(widget_data->info.field.history);
			}

			freeml(dlg_data->ml);
	}
}

int
check_dialog(struct dialog_data *dlg_data)
{
	int i;

	for (i = 0; i < dlg_data->n; i++) {
		struct widget_data *widget_data = &dlg_data->widgets_data[i];

		if (widget_data->widget->type != WIDGET_CHECKBOX &&
		    !widget_is_textfield(widget_data))
			continue;

		if (widget_data->widget->fn &&
		    widget_data->widget->fn(dlg_data, widget_data)) {
			dlg_data->selected = i;
			redraw_dialog(dlg_data);
			return 1;
		}
	}

	return 0;
}

int
cancel_dialog(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	delete_window(dlg_data->win);
	return 0;
}

int
update_dialog_data(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	int i;

	for (i = 0; i < dlg_data->n; i++)
		memcpy(dlg_data->dlg->widgets[i].data,
		       dlg_data->widgets_data[i].cdata,
		       dlg_data->dlg->widgets[i].datalen);

	return 0;
}

int
ok_dialog(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	void (*fn)(void *) = dlg_data->dlg->refresh;
	void *data = dlg_data->dlg->refresh_data;

	if (check_dialog(dlg_data)) return 1;

	update_dialog_data(dlg_data, widget_data);

	if (fn) fn(data);
	return cancel_dialog(dlg_data, widget_data);
}

/* Clear text fields in dialog. */
int
clear_dialog(struct dialog_data *dlg_data, struct widget_data *unused)
{
	int i;

	for (i = 0; i < dlg_data->n; i++) {
		struct widget_data *widget_data = &dlg_data->widgets_data[i];

		if (!widget_is_textfield(widget_data))
			continue;
		memset(widget_data->cdata, 0, widget_data->widget->datalen);
		widget_data->info.field.cpos = 0;
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
	draw_area(dlg_data->win->term, dlg_data->x, dlg_data->y, dlg_data->xw,
		  dlg_data->yw, ' ', 0,
		  get_bfu_color(dlg_data->win->term, "dialog.generic"));

	if (get_opt_bool("ui.dialogs.shadows")) {
		/* Draw shadow */
		struct color_pair *shadow_color = get_bfu_color(dlg_data->win->term,
								"dialog.shadow");

		/* (horizontal) */
		draw_area(dlg_data->win->term, dlg_data->x + 2, dlg_data->y + dlg_data->yw,
			  dlg_data->xw - 2, 1, ' ', 0, shadow_color);

		/* (vertical) */
		draw_area(dlg_data->win->term, dlg_data->x + dlg_data->xw, dlg_data->y + 1,
			  2, dlg_data->yw, ' ', 0, shadow_color);
	}
}
