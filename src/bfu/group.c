/* Widget group implementation. */
/* $Id: group.c,v 1.4 2002/08/07 02:56:58 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/align.h"
#include "bfu/colors.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/group.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"


/* max_group_width() */
void max_group_width(struct terminal *term, unsigned char **texts,
		     struct widget_data *item, int n, int *w)
{
	int ww = 0;

	while (n--) {
		int wx;

		if (item->item->type == D_CHECKBOX) {
			wx = 4;
		} else if (item->item->type == D_BUTTON) {
			wx = strlen(_(item->item->text, term)) + 5;
		} else {
			wx = item->item->dlen + 1;
		}

		wx += strlen(_(texts[0], term));
		if (n) wx++;
		ww += wx;
		texts++;
		item++;
	}

	if (ww > *w) *w = ww;
}

/* min_group_width() */
void min_group_width(struct terminal *term, unsigned char **texts,
		     struct widget_data *item, int n, int *w)
{
	while (n--) {
		int wx;

		if (item->item->type == D_CHECKBOX) {
			wx = 4;
		} else if (item->item->type == D_BUTTON) {
			wx = strlen(_(item->item->text, term)) + 5;
		} else {
			wx = item->item->dlen + 1;
		}

		wx += strlen(_(texts[0], term));
		if (wx > *w) *w = wx;
		texts++;
		item++;
	}
}

/* dlg_format_group() */
void dlg_format_group(struct terminal *term, struct terminal *t2,
		      unsigned char **texts, struct widget_data *item,
		      int n, int x, int *y, int w, int *rw)
{
	int nx = 0;

	while (n--) {
		int sl;
		int wx;

		if (item->item->type == D_CHECKBOX) {
			wx = 4;
		} else if (item->item->type == D_BUTTON) {
			wx = strlen(_(item->item->text, t2)) + 5;
		} else {
			wx = item->item->dlen + 1;
		}

		if (_(texts[0], t2)[0]) {
			sl = strlen(_(texts[0], t2));
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
				   *y, strlen(_(texts[0], t2)),	_(texts[0], t2),
				   get_bfu_color(term, "dialog.text"));
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

/* group_fn() */
void group_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;

	max_group_width(term, dlg->dlg->udata, dlg->items, dlg->n - 2, &max);
	min_group_width(term, dlg->dlg->udata, dlg->items, dlg->n - 2, &min);
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;

	rw = 0;
	dlg_format_group(NULL, term, dlg->dlg->udata, dlg->items, dlg->n - 2,
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
	dlg_format_group(term, term, dlg->dlg->udata, dlg->items, dlg->n - 2,
			 dlg->x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_buttons(term, term, dlg->items + dlg->n - 2, 2,
			   dlg->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}
