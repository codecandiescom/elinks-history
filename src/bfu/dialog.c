/* Dialog box implementation. */
/* $Id: dialog.c,v 1.130 2004/04/16 10:02:06 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"
#include "setup.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/group.h"
#include "bfu/inphist.h"
#include "bfu/listbox.h"
#include "bfu/style.h"
#include "bfu/widget.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "lowlevel/select.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/math.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"

#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"
#include "bfu/text.h"


static void dialog_func(struct window *win, struct term_event *ev, int fwd);

struct dialog_data *
do_dialog(struct terminal *term, struct dialog *dlg,
	  struct memory_list *ml)
{
	struct dialog_data *dlg_data;

	dlg_data = mem_alloc(sizeof(struct dialog_data) +
			     sizeof(struct widget_data) * dlg->widgets_size);
	if (!dlg_data) {
		/* Worry not: freeml() checks whether its argument is NULL. */
		freeml(ml);
		return NULL;
	}

	dlg_data->dlg = dlg;
	dlg_data->n = dlg->widgets_size;
	dlg_data->ml = ml;
	add_window(term, dialog_func, dlg_data);

	return dlg_data;
}

static inline void cycle_widget_focus(struct dialog_data *dlg_data, int direction);

void
redraw_dialog(struct dialog_data *dlg_data, int layout)
{
	int i, x, y;
	struct terminal *term = dlg_data->win->term;
	struct color_pair *title_color;

	if (layout) {
		dlg_data->dlg->layouter(dlg_data);
		/* This might not be the best place. We need to be able
		 * to make focusability of widgets dynamic so widgets
		 * like scrollable text don't receive focus when there
		 * is nothing to scroll. */
		if (!widget_is_focusable(selected_widget(dlg_data)))
			cycle_widget_focus(dlg_data, 1);
	}

	if (!dlg_data->dlg->layout.only_widgets) {
		x = dlg_data->x + DIALOG_LEFT_BORDER;
		y = dlg_data->y + DIALOG_TOP_BORDER;

		draw_border(term, x, y,
			    dlg_data->width - 2 * DIALOG_LEFT_BORDER,
			    dlg_data->height - 2 * DIALOG_TOP_BORDER,
			    get_bfu_color(term, "dialog.frame"),
			    DIALOG_FRAME);

		assert(dlg_data->dlg->title);

		title_color = get_bfu_color(term, "dialog.title");
		if (title_color && dlg_data->width >= 2) {
			unsigned char *title = dlg_data->dlg->title;
			int titlelen = int_min(dlg_data->width - 2, strlen(title));

			x = (dlg_data->width - titlelen) / 2 + dlg_data->x;
			draw_text(term, x - 1, y, " ", 1, 0, title_color);
			draw_text(term, x, y, title, titlelen, 0, title_color);
			draw_text(term, x + titlelen, y, " ", 1, 0, title_color);
		}
	}

	for (i = 0; i < dlg_data->n; i++)
		display_dlg_item(dlg_data, &dlg_data->widgets_data[i],
				 (i == dlg_data->selected));

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
	&checkbox_ops,
	&field_ops,
	&field_pass_ops,
	&button_ops,
	&listbox_ops,
	&text_ops,
};

static inline struct widget_data *
init_widget(struct dialog_data *dlg_data, struct term_event *ev, int i)
{
	struct widget_data *widget_data = &dlg_data->widgets_data[i];

	memset(widget_data, 0, sizeof(struct widget_data));
	widget_data->widget = &dlg_data->dlg->widgets[i];

	if (widget_data->widget->datalen) {
		widget_data->cdata = mem_alloc(widget_data->widget->datalen);
		if (!widget_data->cdata) {
			return NULL;
		}
		memcpy(widget_data->cdata,
		       widget_data->widget->data,
		       widget_data->widget->datalen);
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

static inline void
cycle_widget_focus(struct dialog_data *dlg_data, int direction)
{
	int prev_selected = dlg_data->selected;

	display_dlg_item(dlg_data, selected_widget(dlg_data), 0);

	do {
		dlg_data->selected += direction;

		if (dlg_data->selected >= dlg_data->n)
			dlg_data->selected = 0;
		else if (dlg_data->selected < 0)
			dlg_data->selected = dlg_data->n - 1;

	} while (!widget_is_focusable(selected_widget(dlg_data))
		 && dlg_data->selected != prev_selected);


	display_dlg_item(dlg_data, selected_widget(dlg_data), 1);
	redraw_from_window(dlg_data->win);
}

/* TODO: This is too long and ugly. Rewrite and split. */
static void
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
			for (i = dlg_data->n - 1; i >= 0; i--) {
				struct widget_data *widget_data;

				widget_data = init_widget(dlg_data, ev, i);

				/* Make sure the selected widget is focusable */
				if (widget_data
				    && widget_is_focusable(widget_data))
					dlg_data->selected = i;
			}

		case EV_RESIZE:
		case EV_REDRAW:
			redraw_dialog(dlg_data, 1);
			break;

		case EV_MOUSE:
#ifdef CONFIG_MOUSE
			for (i = 0; i < dlg_data->n; i++)
				if (dlg_data->widgets_data[i].widget->ops->mouse
				    && dlg_data->widgets_data[i].widget->ops->mouse(&dlg_data->widgets_data[i], dlg_data, ev)
				       == EVENT_PROCESSED)
					break;
#endif /* CONFIG_MOUSE */
			break;

		case EV_KBD:
			{
			struct widget_data *widget_data = selected_widget(dlg_data);
			/* XXX: KM_EDIT ? --pasky */
			enum menu_action action = kbd_action(KM_MENU, ev, NULL);

			/* First let the widget try out. */
			if (widget_data->widget->ops->kbd
			    && widget_data->widget->ops->kbd(widget_data, dlg_data, ev)
			       == EVENT_PROCESSED)
				break;

			/* Can we select? */
			if ((action == ACT_MENU_ENTER || action == ACT_MENU_SELECT)
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
			if (action == ACT_MENU_ENTER
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
			if (action == ACT_MENU_CANCEL) {
				for (i = 0; i < dlg_data->n; i++)
					if (dlg_data->dlg->widgets[i].type == WIDGET_BUTTON
					    && dlg_data->dlg->widgets[i].info.button.flags & B_ESC) {
						select_dlg_item(dlg_data, i);
						return;
					}
			}

			/* Cycle focus. */

			if ((action == ACT_MENU_NEXT_ITEM)
			    || action == ACT_MENU_DOWN || action == ACT_MENU_RIGHT) {
				cycle_widget_focus(dlg_data, 1);
				break;
			}

			if ((action == ACT_MENU_PREVIOUS_ITEM)
			    || action == ACT_MENU_UP || action == ACT_MENU_LEFT) {
				cycle_widget_focus(dlg_data, -1);
				break;
			}

			if (action == ACT_MENU_REDRAW) {
				redraw_terminal_cls(win->term);
			}

			break;
			}

		case EV_ABORT:
			if (dlg_data->dlg->refresh) {
				struct dialog_refresh *refresh = dlg_data->dlg->refresh;

				if (refresh->timer != -1)
					kill_timer(refresh->timer);
				mem_free(refresh);
			}

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
			redraw_dialog(dlg_data, 0);
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
	void (*done)(void *) = widget_data->widget->info.button.done;
	void *done_data = widget_data->widget->info.button.done_data;

	if (check_dialog(dlg_data)) return 1;

	update_dialog_data(dlg_data, widget_data);

	if (done) done(done_data);
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

	redraw_dialog(dlg_data, 0);
	return 0;
}


static inline void
format_widgets(struct terminal *term, struct dialog_data *dlg_data,
	       int x, int *y, int w, int h, int *rw)
{
	struct widget_data *wdata = dlg_data->widgets_data;
	int widgets = dlg_data->n;

	/* TODO: Do something if (*y) gets > height. */
	for (; widgets > 0; widgets--, wdata++, (*y)++) {
		switch (wdata->widget->type) {
		case WIDGET_FIELD_PASS:
		case WIDGET_FIELD:
			dlg_format_field(term, wdata, x, y, w, rw, AL_LEFT);
			break;

		case WIDGET_LISTBOX:
			dlg_format_box(term, wdata, x, y, w, h, rw, AL_LEFT);
			break;

		case WIDGET_TEXT:
			dlg_format_text(term, wdata, x, y, w, rw, h);
			break;

		case WIDGET_CHECKBOX:
		{
			int group = widget_has_group(wdata);

			if (group > 0 && dlg_data->dlg->layout.float_groups) {
				int size;

				/* Find group size */
				for (size = 1; widgets > 0; size++, widgets--) {
					struct widget_data *next = &wdata[size];

					if (group != widget_has_group(next))
						break;
				}

				dlg_format_group(term, wdata, size, x, y, w, rw);
				wdata += size - 1;

			} else {

				/* No horizontal space between checkboxes belonging to
				 * the same group. */
				dlg_format_checkbox(term, wdata, x, y, w, rw, AL_LEFT);
				if (widgets > 1
				    && group == widget_has_group(&wdata[1]))
					(*y)--;
			}
		}
			break;

		/* We assume that the buttons are all stuffed at the very end
		 * of the dialog. */
		case WIDGET_BUTTON:
			dlg_format_buttons(term, wdata, widgets,
					   x, y, w, rw, AL_CENTER);
			return;
		}
	}
}

void
generic_dialog_layouter(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int w = dialog_max_width(term);
	int height = dialog_max_height(term);
	int rw = int_min(w, strlen(dlg_data->dlg->title));
	int y = dlg_data->dlg->layout.padding_top ? 0 : -1;
	int x = 0;

	format_widgets(NULL, dlg_data, x, &y, w, height, &rw);

	/* Update the width to respond to the required minimum width */
	if (dlg_data->dlg->layout.fit_datalen) {
		int_lower_bound(&rw, dlg_data->dlg->widgets->datalen);
		int_upper_bound(&w, rw);
	} else if (!dlg_data->dlg->layout.maximize_width) {
		w = rw;
	}

	draw_dialog(dlg_data, w, y);

	y = dlg_data->y + DIALOG_TB + dlg_data->dlg->layout.padding_top;
	x = dlg_data->x + DIALOG_LB;

	format_widgets(term, dlg_data, x, &y, w, height, NULL);
}


void
draw_dialog(struct dialog_data *dlg_data, int width, int height)
{
	struct terminal *term = dlg_data->win->term;

	dlg_data->width = int_min(term->width, width + 2 * DIALOG_LB);
	dlg_data->height = int_min(term->height, height + 2 * DIALOG_TB);

	dlg_data->x = (term->width - dlg_data->width) / 2;
	dlg_data->y = (term->height - dlg_data->height) / 2;

	draw_area(term, dlg_data->x, dlg_data->y,
		  dlg_data->width, dlg_data->height, ' ', 0,
		  get_bfu_color(term, "dialog.generic"));

	if (get_opt_bool("ui.dialogs.shadows")) {
		/* Draw shadow */
		struct color_pair *shadow_color = get_bfu_color(term,
								"dialog.shadow");

		/* (horizontal) */
		draw_area(term, dlg_data->x + 2, dlg_data->y + dlg_data->height,
			  dlg_data->width - 2, 1, ' ', 0, shadow_color);

		/* (vertical) */
		draw_area(term, dlg_data->x + dlg_data->width, dlg_data->y + 1,
			  2, dlg_data->height, ' ', 0, shadow_color);
	}
}

static void
do_refresh_dialog(struct dialog_data *dlg_data)
{
	struct dialog_refresh *refresh = dlg_data->dlg->refresh;
	enum dlg_refresh_code refresh_code;

	assert(refresh && refresh->handler);

	refresh_code = refresh->handler(dlg_data, refresh->data);

	if (refresh_code == REFRESH_CANCEL
	    || refresh_code == REFRESH_STOP) {
		refresh->timer = -1;
		if (refresh_code == REFRESH_CANCEL)
			cancel_dialog(dlg_data, NULL);
		return;
	}

	/* We want dialog_has_refresh() to be true while drawing
	 * so we can not set the timer to -1. */
	if (refresh_code == REFRESH_DIALOG) {
		redraw_dialog(dlg_data, 1);
	}

	refresh->timer = install_timer(RESOURCE_INFO_REFRESH,
				(void (*)(void *)) do_refresh_dialog, dlg_data);
}

void
refresh_dialog(struct dialog_data *dlg_data, dialog_refresh_handler handler, void *data)
{
	struct dialog_refresh *refresh = dlg_data->dlg->refresh;

	if (!refresh) {
		refresh = mem_calloc(1, sizeof(struct dialog_refresh));
		if (!refresh) return;

		dlg_data->dlg->refresh = refresh;

	} else if (refresh->timer != -1) {
		kill_timer(refresh->timer);
	}

	refresh->handler = handler;
	refresh->data = data;
	refresh->timer = install_timer(RESOURCE_INFO_REFRESH,
				(void (*)(void *)) do_refresh_dialog, dlg_data);
}
