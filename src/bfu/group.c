/* Widget group implementation. */
/* $Id: group.c,v 1.16 2003/06/07 17:53:09 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/group.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"


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

	if (ww > *w) *w = ww;
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

		if (wx > wt) wt = wx;
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
	int dialog_text_color = get_bfu_color(term, "dialog.text");

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
			print_text(term, x + nx + 4 * (item->item->type == D_CHECKBOX),
				   *y, ((sl == -1) ? strlen(text) : sl) /* hmmm... */, text,
				   dialog_text_color);
			item->x = x + nx + (sl + 1) * (item->item->type != D_CHECKBOX);
			item->y = *y;
			if (item->item->type == D_FIELD ||
			    item->item->type == D_FIELD_PASS)
				item->l = item->item->dlen;
		}

		if (rw && nx + wx > *rw) {
			*rw = nx + wx;
			if (*rw > w) *rw = w;
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

	max_group_width(term, 1, dlg->dlg->udata, dlg->items, dlg->n - 2, &max);
	min_group_width(term, 1, dlg->dlg->udata, dlg->items, dlg->n - 2, &min);
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;

	rw = 0;
	dlg_format_group(NULL, term, 1, dlg->dlg->udata, dlg->items, dlg->n - 2,
			 0, &y, w, &rw);

	y++;
	dlg_format_buttons(NULL, term, dlg->items + dlg->n - 2, 2, 0, &y, w,
			   &rw, AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB + 1;
	dlg_format_group(term, term, 1, dlg->dlg->udata, dlg->items, dlg->n - 2,
			 dlg->x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_buttons(term, term, dlg->items + dlg->n - 2, 2,
			   dlg->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}
