/* Checkbox widget handlers. */
/* $Id: checkbox.c,v 1.69 2004/05/07 11:24:20 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/dialog.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"


void
dlg_format_checkbox(struct terminal *term,
		    struct widget_data *widget_data,
		    int x, int *y, int w, int *rw,
		    enum format_align align)
{
	unsigned char *text = widget_data->widget->text;

	if (term) {
		widget_data->dimensions.x = x;
		widget_data->dimensions.y = *y;
	}

	if (w <= 4) return;

	if (rw) *rw -= 4;
	dlg_format_text_do(term, text, x + 4, y, w - 4, rw,
			get_bfu_color(term, "dialog.checkbox-label"), align);
	if (rw) *rw += 4;
}

static void
display_checkbox(struct widget_data *widget_data, struct dialog_data *dlg_data, int sel)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color;
	unsigned char *text;

	color = get_bfu_color(term, "dialog.checkbox");
	if (!color) return;

	if (widget_data->info.checkbox.checked) {
		text = (!widget_data->widget->info.checkbox.gid) ? "[X]" : "(X)";
	} else {
		text = (!widget_data->widget->info.checkbox.gid) ? "[ ]" : "( )";
	}

	draw_text(term, widget_data->dimensions.x, widget_data->dimensions.y, text, 3, 0, color);

	if (sel) {
		set_cursor(term, widget_data->dimensions.x + 1, widget_data->dimensions.y, 0);
		set_window_ptr(dlg_data->win, widget_data->dimensions.x, widget_data->dimensions.y);
	}
}

static void
init_checkbox(struct widget_data *widget_data, struct dialog_data *dlg_data,
	      struct term_event *ev)
{
	if (widget_data->widget->info.checkbox.gid) {
		if (*((int *) widget_data->cdata) == widget_data->widget->info.checkbox.gnum)
			widget_data->info.checkbox.checked = 1;
	} else {
		if (*((int *) widget_data->cdata))
			widget_data->info.checkbox.checked = 1;
	}
}

static int
mouse_checkbox(struct widget_data *widget_data, struct dialog_data *dlg_data,
	       struct term_event *ev)
{
	if (check_mouse_wheel(ev)
	    || ev->y != widget_data->dimensions.y
	    || ev->x < widget_data->dimensions.x
	    || ev->x >= widget_data->dimensions.x + 3)
		return EVENT_NOT_PROCESSED;
	display_dlg_item(dlg_data, selected_widget(dlg_data), 0);
	dlg_data->selected = widget_data - dlg_data->widgets_data;
	display_dlg_item(dlg_data, widget_data, 1);
	if (check_mouse_action(ev, B_UP) && widget_data->widget->ops->select)
		widget_data->widget->ops->select(widget_data, dlg_data);
	return EVENT_PROCESSED;
}

static void
select_checkbox(struct widget_data *widget_data, struct dialog_data *dlg_data)
{
	if (!widget_data->widget->info.checkbox.gid) {
		widget_data->info.checkbox.checked = *((int *) widget_data->cdata)
			    = !*((int *) widget_data->cdata);
	} else {
		int i;

		for (i = 0; i < dlg_data->n; i++) {
			if (dlg_data->widgets_data[i].widget->type != WIDGET_CHECKBOX
			    || dlg_data->widgets_data[i].widget->info.checkbox.gid
			       != widget_data->widget->info.checkbox.gid)
				continue;

			*((int *) dlg_data->widgets_data[i].cdata) = widget_data->widget->info.checkbox.gnum;
			dlg_data->widgets_data[i].info.checkbox.checked = 0;
			display_dlg_item(dlg_data, &dlg_data->widgets_data[i], 0);
		}
		widget_data->info.checkbox.checked = 1;
	}
	display_dlg_item(dlg_data, widget_data, 1);
}

struct widget_ops checkbox_ops = {
	display_checkbox,
	init_checkbox,
	mouse_checkbox,
	NULL,
	select_checkbox,
};
