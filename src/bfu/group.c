/* Widget group implementation. */
/* $Id: group.c,v 1.31 2003/10/26 15:59:12 zas Exp $ */

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
	if (widget_data->widget->type == D_CHECKBOX)
		return 4;

	if (widget_data->widget->type == D_BUTTON)
		return strlen(widget_data->widget->text) + 5;

	return widget_data->widget->datalen + 1;
}

/* TODO: We should join these two functions in one. --Zas */
inline void
max_group_width(struct terminal *term, int intl, unsigned char **texts,
		struct widget_data *widget_data, int n, int *w)
{
	int ww = 0;
	int base = base_group_width(term, widget_data);

	while (n--) {
		int wx;
		unsigned char *text = texts[0];

		if (intl) text = _(text, term);
		wx = base + strlen(text);

		if (n) wx++;
		ww += wx;
		texts++;
		widget_data++;
	}

	int_lower_bound(w, ww);
}

inline void
min_group_width(struct terminal *term, int intl, unsigned char **texts,
		struct widget_data *widget_data, int n, int *w)
{
	int base = base_group_width(term, widget_data);
	int wt = 0;

	while (n--) {
		int wx;
		unsigned char *text = texts[0];

		if (intl) text = _(text, term);
		wx = strlen(text);

		int_lower_bound(&wt, wx);
		texts++;
		widget_data++;
	}

	*w = wt + base;
}

void
dlg_format_group(struct terminal *term, struct terminal *t2, int intl,
		 unsigned char **texts, struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw)
{
	int nx = 0;
	int base = base_group_width(t2, widget_data);
	struct color_pair *color = get_bfu_color(term, "dialog.text");

	while (n--) {
		int sl;
		int wx = base;
		unsigned char *text = texts[0];

		if (intl) text = _(text, t2);

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
			int is_checkbox = (widget_data->widget->type == D_CHECKBOX);
			int xnx = x + nx;

			draw_text(term, xnx + 4 * is_checkbox, *y,
				  text, ((sl == -1) ? strlen(text) : sl),
				  0, color);
			widget_data->x = xnx + !is_checkbox * (sl + 1);
			widget_data->y = *y;
			if (widget_data->widget->type == D_FIELD ||
			    widget_data->widget->type == D_FIELD_PASS)
				widget_data->w = widget_data->widget->datalen;
		}

		if (rw) int_bounds(rw, nx + wx, w);

		nx += wx + 1;
		texts++;
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

	max_group_width(term, 1, dlg_data->dlg->udata, dlg_data->widgets_data, n, &max);
	min_group_width(term, 1, dlg_data->dlg->udata, dlg_data->widgets_data, n, &min);
	buttons_width(term, dlg_data->widgets_data + n, 2, &min, &max);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	int_bounds(&w, min, max);
	int_bounds(&w, 1, term->x - 2 * DIALOG_LB);

	rw = 0;
	dlg_format_group(NULL, term, 1, dlg_data->dlg->udata, dlg_data->widgets_data, n,
			 0, &y, w, &rw);

	y++;
	dlg_format_buttons(NULL, term, dlg_data->widgets_data + n, 2, 0, &y, w,
			   &rw, AL_CENTER);

	w = rw;
	dlg_data->xw = rw + 2 * DIALOG_LB;
	dlg_data->yw = y + 2 * DIALOG_TB;

	center_dlg(dlg_data);
	draw_dlg(dlg_data);

	y = dlg_data->y + DIALOG_TB + 1;
	dlg_format_group(term, term, 1, dlg_data->dlg->udata, dlg_data->widgets_data, n,
			 dlg_data->x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_buttons(term, term, dlg_data->widgets_data + n, 2,
			   dlg_data->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}
