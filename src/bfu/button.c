/* Button widget handlers. */
/* $Id: button.c,v 1.49 2004/04/23 20:44:26 pasky Exp $ */

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

static void
buttons_width(struct widget_data *widget_data, int n,
	      int *minwidth, int *maxwidth)
{
	int maxw = -2;
	register int i;

	for (i = 0; i < n; i++) {
		int minw = strlen((widget_data++)->widget->text) + 6;

		maxw += minw;
		if (minwidth) *minwidth = int_max(*minwidth, minw);
	}

	if (maxwidth) *maxwidth = int_max(*maxwidth, maxw);
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
				widget_data[i].x = p;
				widget_data[i].y = *y;
				widget_data[i].w = strlen(widget_data[i].widget->text) + 4;
				p += widget_data[i].w + 2;
			}
		}

		*y += 2;
		i1 = i2;
	}
}


static void
display_button(struct widget_data *widget_data, struct dialog_data *dlg_data, int sel)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color;
	int len = strlen(widget_data->widget->text);
	int x = widget_data->x + 2;

	color = get_bfu_color(term, sel ? "dialog.button-selected"
					: "dialog.button");
	if (!color) return;

	draw_text(term, widget_data->x, widget_data->y, "[ ", 2, 0, color);
	draw_text(term, x, widget_data->y, widget_data->widget->text, len, 0, color);
	draw_text(term, x + len, widget_data->y, " ]", 2, 0, color);

	if (sel) {
		set_cursor(term, x, widget_data->y, 0);
		set_window_ptr(dlg_data->win, widget_data->x, widget_data->y);
	}
}

static int
mouse_button(struct widget_data *widget_data, struct dialog_data *dlg_data, struct term_event *ev)
{
	if (check_mouse_wheel(ev) || ev->y != widget_data->y || ev->x < widget_data->x
	    || ev->x >= widget_data->x + strlen(widget_data->widget->text) + 4)
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
