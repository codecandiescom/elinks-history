/* Widget group implementation. */
/* $Id: group.c,v 1.23 2003/08/29 14:30:24 zas Exp $ */

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
base_group_width(struct terminal *term, struct widget_data *item)
{
	if (item->item->type == D_CHECKBOX)
		return 4;

	if (item->item->type == D_BUTTON)
		return strlen(item->item->text) + 5;

	return item->item->dlen + 1;
}

/* TODO: We should join these two functions in one. --Zas */
static inline void
max_group_width(struct terminal *term, int intl, unsigned char **texts,
		struct widget_data *item, int n, int *w)
{
	int ww = 0;
	int base = base_group_width(term, item);

	while (n--) {
		int wx;
		unsigned char *text = texts[0];

		if (intl) text = _(text, term);
		wx = base + strlen(text);

		if (n) wx++;
		ww += wx;
		texts++;
		item++;
	}

	int_lower_bound(w, ww);
}

static inline void
min_group_width(struct terminal *term, int intl, unsigned char **texts,
		struct widget_data *item, int n, int *w)
{
	int base = base_group_width(term, item);
	int wt = 0;

	while (n--) {
		int wx;
		unsigned char *text = texts[0];

		if (intl) text = _(text, term);
		wx = strlen(text);

		int_lower_bound(&wt, wx);
		texts++;
		item++;
	}

	*w = wt + base;
}

static void
dlg_format_group(struct terminal *term, struct terminal *t2, int intl,
		 unsigned char **texts, struct widget_data *item,
		 int n, int x, int *y, int w, int *rw)
{
	int nx = 0;
	int base = base_group_width(t2, item);
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
			int is_checkbox = (item->item->type == D_CHECKBOX);
			int xnx = x + nx;

			draw_text(term, xnx + 4 * is_checkbox, *y,
				  text, ((sl == -1) ? strlen(text) : sl),
				  0, color);
			item->x = xnx + !is_checkbox * (sl + 1);
			item->y = *y;
			if (item->item->type == D_FIELD ||
			    item->item->type == D_FIELD_PASS)
				item->l = item->item->dlen;
		}

		if (rw) {
			int_lower_bound(rw, nx + wx);
			int_upper_bound(rw, w);
		}
		nx += wx + 1;
		texts++;
		item++;
	}
	(*y)++;
}

void
group_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	int n = dlg->n - 2;

	max_group_width(term, 1, dlg->dlg->udata, dlg->items, n, &max);
	min_group_width(term, 1, dlg->dlg->udata, dlg->items, n, &min);
	buttons_width(term, dlg->items + n, 2, &min, &max);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	int_upper_bound(&w, max);
	int_lower_bound(&w, min);
	int_upper_bound(&w, term->x - 2 * DIALOG_LB);
	int_lower_bound(&w, 1);

	rw = 0;
	dlg_format_group(NULL, term, 1, dlg->dlg->udata, dlg->items, n,
			 0, &y, w, &rw);

	y++;
	dlg_format_buttons(NULL, term, dlg->items + n, 2, 0, &y, w,
			   &rw, AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB + 1;
	dlg_format_group(term, term, 1, dlg->dlg->udata, dlg->items, n,
			 dlg->x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_buttons(term, term, dlg->items + n, 2,
			   dlg->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}
