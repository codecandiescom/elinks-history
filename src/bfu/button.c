/* Button widget handlers. */
/* $Id: button.c,v 1.52 2004/05/10 12:56:13 zas Exp $ */

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
#include "terminal/kbd.h"
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
	register int i;

	for (i = 0; i < n; i++) {
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
			int p = x + (align == AL_CENTER ? (w - mw) / 2 : 0);

			for (i = i1; i < i2; i++) {
				set_rect(&widget_data[i].dimensions,
					 p, *y,
					 strlen(widget_data[i].widget->text) + BUTTON_LR_LEN, 1);

				p += widget_data[i].dimensions.width + BUTTON_HSPACING;
			}
		}

		*y += BUTTON_VSPACING + 1 /* height of button */;
		i1 = i2;
	}
}

static void
display_button(struct widget_data *widget_data, struct dialog_data *dlg_data, int sel)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color;
	int len = strlen(widget_data->widget->text);
	int x = widget_data->dimensions.x + BUTTON_LEFT_LEN;

	color = get_bfu_color(term, sel ? "dialog.button-selected"
					: "dialog.button");
	if (!color) return;

	draw_text(term, widget_data->dimensions.x, widget_data->dimensions.y, BUTTON_LEFT, BUTTON_LEFT_LEN, 0, color);
	draw_text(term, x, widget_data->dimensions.y, widget_data->widget->text, len, 0, color);
	draw_text(term, x + len, widget_data->dimensions.y, BUTTON_RIGHT, BUTTON_RIGHT_LEN, 0, color);

	if (sel) {
		set_cursor(term, x, widget_data->dimensions.y, 0);
		set_window_ptr(dlg_data->win, widget_data->dimensions.x, widget_data->dimensions.y);
	}
}

static int
mouse_button(struct widget_data *widget_data, struct dialog_data *dlg_data, struct term_event *ev)
{
	if (check_mouse_wheel(ev))
		return EVENT_NOT_PROCESSED;

	if (!is_in_rect(&widget_data->dimensions, ev->x, ev->y))
		return EVENT_NOT_PROCESSED;

	display_dlg_item(dlg_data, selected_widget(dlg_data), 0);
	dlg_data->selected = widget_data - dlg_data->widgets_data;
	display_dlg_item(dlg_data, widget_data, 1);
	if (check_mouse_action(ev, B_UP) && widget_data->widget->ops->select)
		widget_data->widget->ops->select(widget_data, dlg_data);
	return EVENT_PROCESSED;
}

static void
select_button(struct widget_data *widget_data, struct dialog_data *dlg_data)
{
	widget_data->widget->fn(dlg_data, widget_data);
}

struct widget_ops button_ops = {
	display_button,
	NULL,
	mouse_button,
	NULL,
	select_button,
};
