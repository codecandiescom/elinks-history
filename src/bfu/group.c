/* Widget group implementation. */
/* $Id: group.c,v 1.48 2003/11/07 18:45:39 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/group.h"
#include "bfu/style.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/color.h"


static inline int
base_group_width(struct widget_data *widget_data)
{
	if (widget_data->widget->type == WIDGET_CHECKBOX)
		return 4;

	if (widget_data->widget->type == WIDGET_BUTTON)
		return strlen(widget_data->widget->text) + 5;

	return widget_data->widget->datalen + 1;
}

void
dlg_format_group(struct terminal *term,
		 struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw)
{
	int nx = 0;
	int base = base_group_width(widget_data);
	struct color_pair *color = get_bfu_color(term, "dialog.text");

	while (n--) {
		int sl;
		int wx = base;
		unsigned char *text = widget_data->widget->text;

		if (text[0]) {
			sl = strlen(text);
		} else {
			sl = -1;
		}

		wx += sl;
		if (nx && nx + wx > w) {
			nx = 0;
			(*y) += 2;
		}

		if (term) {
			int is_checkbox = (widget_data->widget->type == WIDGET_CHECKBOX);
			int xnx = x + nx;

			draw_text(term, xnx + 4 * is_checkbox, *y,
				  text, ((sl == -1) ? strlen(text) : sl),
				  0, color);
			widget_data->x = xnx + !is_checkbox * (sl + 1);
			widget_data->y = *y;
			if (widget_is_textfield(widget_data))
				widget_data->w = widget_data->widget->datalen;
		}

		if (rw) int_bounds(rw, nx + wx, w);

		nx += wx + 1;
		widget_data++;
	}
	(*y)++;
}

void
group_layouter(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int w = dialog_max_width(term);
	int rw = 0;
	int y = 0;
	int n = dlg_data->n - 2;

	dlg_format_group(NULL, dlg_data->widgets_data, n,
			 0, &y, w, &rw);

	y++;
	dlg_format_buttons(NULL, dlg_data->widgets_data + n, 2, 0, &y, w,
			   &rw, AL_CENTER);

	w = rw;

	draw_dialog(dlg_data, w, y, AL_CENTER);

	y = dlg_data->y + DIALOG_TB + 1;
	dlg_format_group(term, dlg_data->widgets_data, n,
			 dlg_data->x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_buttons(term, dlg_data->widgets_data + n, 2,
			   dlg_data->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}
