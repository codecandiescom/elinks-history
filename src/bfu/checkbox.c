/* Checkbox widget handlers. */
/* $Id: checkbox.c,v 1.74 2004/07/27 16:11:42 jonas Exp $ */

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
#include "terminal/mouse.h"
#include "terminal/terminal.h"


#define CHECKBOX_LEN 3	/* "[X]" or "(X)" */
#define CHECKBOX_SPACING 1	/* "[X]" + " " + "Label" */
#define CHECKBOX_LS (CHECKBOX_LEN + CHECKBOX_SPACING)	/* "[X] " */

void
dlg_format_checkbox(struct terminal *term,
		    struct widget_data *widget_data,
		    int x, int *y, int w, int *rw,
		    enum format_align align)
{
	unsigned char *text = widget_data->widget->text;

	set_box(&widget_data->box, x, *y, CHECKBOX_LS, 1);

	if (w <= CHECKBOX_LS) return;

	if (rw) *rw -= CHECKBOX_LS;
	dlg_format_text_do(term, text, x + CHECKBOX_LS, y, w - CHECKBOX_LS, rw,
			get_bfu_color(term, "dialog.checkbox-label"), align);
	if (rw) *rw += CHECKBOX_LS;

}

static void
display_checkbox(struct widget_data *widget_data, struct dialog_data *dlg_data,
		 int selected)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color;
	unsigned char *text;
	struct box *pos = &widget_data->box;

	color = get_bfu_color(term, "dialog.checkbox");
	if (!color) return;

	if (widget_data->info.checkbox.checked)
		text = widget_data->widget->info.checkbox.gid ? "(X)" : "[X]";
	else
		text = widget_data->widget->info.checkbox.gid ? "( )" : "[ ]";

	draw_text(term, pos->x, pos->y, text, CHECKBOX_LEN, 0, color);

	if (selected) {
		set_cursor(term, pos->x + 1, pos->y, 0);
		set_window_ptr(dlg_data->win, pos->x, pos->y);
	}
}

static void
init_checkbox(struct widget_data *widget_data, struct dialog_data *dlg_data,
	      struct term_event *ev)
{
	int *cdata = (int *) widget_data->cdata;

	assert(cdata);

	if (widget_data->widget->info.checkbox.gid) {
		if (*cdata == widget_data->widget->info.checkbox.gnum)
			widget_data->info.checkbox.checked = 1;
	} else {
		if (*cdata)
			widget_data->info.checkbox.checked = 1;
	}
}

static int
mouse_checkbox(struct widget_data *widget_data, struct dialog_data *dlg_data,
	       struct term_event *ev)
{
	if (check_mouse_wheel(ev)
	    || !check_mouse_position(ev, &widget_data->box))
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
		/* Checkbox. */
		int *cdata = (int *) widget_data->cdata;

		assert(cdata);

		widget_data->info.checkbox.checked = *cdata = !*cdata;

	} else {
		/* Group of radio buttons. */
		int i;

		for (i = 0; i < dlg_data->n; i++) {
			struct widget_data *wdata = &dlg_data->widgets_data[i];
			int *cdata = (int *) wdata->cdata;

			if (wdata->widget->type != WIDGET_CHECKBOX)
				continue;

			if (wdata->widget->info.checkbox.gid
			    != widget_data->widget->info.checkbox.gid)
				continue;

			assert(cdata);

			*cdata = widget_data->widget->info.checkbox.gnum;
			wdata->info.checkbox.checked = 0;
			display_dlg_item(dlg_data, wdata, 0);
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
