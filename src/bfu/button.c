/* Button widget handlers. */
/* $Id: button.c,v 1.67 2004/11/17 23:02:07 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/style.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"

/* Vertical spacing between buttons. */
#define BUTTON_VSPACING	1

/* Horizontal spacing between buttons. */
#define BUTTON_HSPACING	2

/* Left and right text appearing around label of button.
 * Currently a dialog button is displayed as [ LABEL ] */
#define BUTTON_LEFT "[ "
#define BUTTON_RIGHT " ]"
#define BUTTON_LEFT_LEN (sizeof(BUTTON_LEFT) - 1)
#define BUTTON_RIGHT_LEN (sizeof(BUTTON_RIGHT) - 1)

#define BUTTON_LR_LEN (BUTTON_LEFT_LEN + BUTTON_RIGHT_LEN)

static void
buttons_width(struct widget_data *widget_data, int n,
	      int *minwidth, int *maxwidth)
{
	int maxw = -BUTTON_HSPACING;

	assert(n > 0);
	if_assert_failed return;

	while (n--) {
		int minw = strlen((widget_data++)->widget->text)
			   + BUTTON_HSPACING + BUTTON_LR_LEN;

		maxw += minw;
		if (minwidth) int_lower_bound(minwidth, minw);
	}

	if (maxwidth) int_lower_bound(maxwidth, maxw);
}

void
dlg_format_buttons(struct terminal *term,
		   struct widget_data *widget_data, int n,
		   int x, int *y, int w, int *rw, enum format_align align)
{
	int i1 = 0;

	while (i1 < n) {
		struct widget_data *widget_data1 = widget_data + i1;
		int i2 = i1 + 1;
		int mw;

		while (i2 < n) {
			mw = 0;
			buttons_width(widget_data1, i2 - i1 + 1, NULL, &mw);
			if (mw <= w) i2++;
			else break;
		}

		mw = 0;
		buttons_width(widget_data1, i2 - i1, NULL, &mw);
		if (rw) int_bounds(rw, mw, w);

		if (term) {
			int i;
			int p = x + (align == ALIGN_CENTER ? (w - mw) / 2 : 0);

			for (i = i1; i < i2; i++) {
				set_box(&widget_data[i].box,
					 p, *y,
					 strlen(widget_data[i].widget->text) + BUTTON_LR_LEN, 1);

				p += widget_data[i].box.width + BUTTON_HSPACING;
			}
		}

		*y += BUTTON_VSPACING + 1; /* height of button */
		i1 = i2;
	}
}

static t_handler_event_status
display_button(struct dialog_data *dlg_data, struct widget_data *widget_data, int sel)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color, *shortcut_color;
	struct box *pos = &widget_data->box;
	int len = widget_data->box.width - BUTTON_LR_LEN;
	int x = pos->x + BUTTON_LEFT_LEN;

	if (sel) {
		shortcut_color = get_bfu_color(term, "dialog.button-shortcut-selected");
		color =  get_bfu_color(term, "dialog.button-selected");
	} else {
		shortcut_color = get_bfu_color(term, "dialog.button-shortcut");
		color =  get_bfu_color(term, "dialog.button");
	}
	if (!color || !shortcut_color) return EVENT_PROCESSED;

	draw_text(term, pos->x, pos->y, BUTTON_LEFT, BUTTON_LEFT_LEN, 0, color);
	if (len > 0) {
		draw_text(term, x, pos->y, widget_data->widget->text, 1, 0, shortcut_color);
		draw_text(term, x + 1, pos->y, &widget_data->widget->text[1], len - 1, 0, color);
	}
	draw_text(term, x + len, pos->y, BUTTON_RIGHT, BUTTON_RIGHT_LEN, 0, color);

	if (sel) {
		set_cursor(term, x, pos->y, 0);
		set_window_ptr(dlg_data->win, pos->x, pos->y);
	}
	return EVENT_PROCESSED;
}

static t_handler_event_status
mouse_button(struct dialog_data *dlg_data, struct widget_data *widget_data,
	     struct term_event *ev)
{
	struct terminal *term = dlg_data->win->term;

	if (check_mouse_wheel(ev))
		return EVENT_NOT_PROCESSED;

	if (!check_mouse_position(ev, &widget_data->box))
		return EVENT_NOT_PROCESSED;

	display_dlg_item(dlg_data, selected_widget(dlg_data), 0);
	dlg_data->selected = widget_data - dlg_data->widgets_data;
	display_dlg_item(dlg_data, widget_data, 1);

	do_not_ignore_next_mouse_event(term);

	if (check_mouse_action(ev, B_UP) && widget_data->widget->ops->select)
		return widget_data->widget->ops->select(dlg_data, widget_data);

	return EVENT_PROCESSED;
}

static t_handler_event_status
select_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	return widget_data->widget->fn(dlg_data, widget_data);
}

struct widget_ops button_ops = {
	display_button,
	NULL,
	mouse_button,
	NULL,
	select_button,
};
