/* Widget group implementation. */
/* $Id: group.c,v 1.41 2003/11/05 20:08:16 jonas Exp $ */

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
base_group_width(struct terminal *term, struct widget_data *widget_data)
{
	if (widget_data->widget->type == WIDGET_CHECKBOX)
		return 4;

	if (widget_data->widget->type == WIDGET_BUTTON)
		return strlen(widget_data->widget->text) + 5;

	return widget_data->widget->datalen + 1;
}

inline void
group_width(struct terminal *term,
  	    struct widget_data *widget_data, int n,
	    int *min_width, int *max_width)
{
	int base = base_group_width(term, widget_data);
	int minw = 0;
	int maxw = 0;

	while (n--) {
		int wx_min;
		int wx_max;
		unsigned char *text = widget_data->widget->text;

		wx_min = strlen(text);
		wx_max = base + wx_min;

		int_lower_bound(&minw, wx_min);
		if (n) wx_max++;
		maxw += wx_max;

		widget_data++;
	}

	if (min_width) *min_width = minw + base;
	if (max_width) int_lower_bound(max_width, maxw);
}

void
dlg_format_group(struct terminal *term, struct terminal *t2,
		 struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw)
{
	int nx = 0;
	int base = base_group_width(t2, widget_data);
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
group_fn(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	int n = dlg_data->n - 2;

	group_width(term, dlg_data->widgets_data, n, &min, &max);
	buttons_width(dlg_data->widgets_data + n, 2, &min, &max);

	w = term->width * 9 / 10 - 2 * DIALOG_LB;
	int_bounds(&w, min, max);
	int_bounds(&w, 1, term->width - 2 * DIALOG_LB);

	rw = 0;
	dlg_format_group(NULL, term, dlg_data->widgets_data, n,
			 0, &y, w, &rw);

	y++;
	dlg_format_buttons(NULL, dlg_data->widgets_data + n, 2, 0, &y, w,
			   &rw, AL_CENTER);

	w = rw;
	dlg_data->width = rw + 2 * DIALOG_LB;
	dlg_data->height = y + 2 * DIALOG_TB;

	center_dlg(dlg_data);
	draw_dlg(dlg_data);

	y = dlg_data->y + DIALOG_TB + 1;
	dlg_format_group(term, term, dlg_data->widgets_data, n,
			 dlg_data->x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_buttons(term, dlg_data->widgets_data + n, 2,
			   dlg_data->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}
