/* Checkbox widget handlers. */
/* $Id: checkbox.c,v 1.28 2003/06/27 20:39:32 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/text.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"


void
dlg_format_checkbox(struct terminal *term, struct terminal *t2,
		    struct widget_data *chkb,
		    int x, int *y, int w, int *rw,
		    unsigned char *text)
{
	if (term) {
		chkb->x = x;
		chkb->y = *y;
	}

	if (rw) *rw -= 4;
	dlg_format_text(term, t2, text, x + 4, y, w - 4, rw,
			get_bfu_color(term, "dialog.checkbox-label"), AL_LEFT);
	if (rw) *rw += 4;
}

void
dlg_format_checkboxes(struct terminal *term, struct terminal *t2, int intl,
		      struct widget_data *chkb, int n,
		      int x, int *y, int w, int *rw,
		      unsigned char **texts)
{
	while (n) {
		unsigned char *text = texts[0];

		if (intl) text = _(text, t2);
		dlg_format_checkbox(term, t2, chkb, x, y, w, rw, text);
		texts++;
		chkb++;
		n--;
	}
}

void
min_max_checkboxes_width(struct terminal *term, int intl, unsigned char **texts,
			 int *minwidth, int *maxwidth)
{
	while (texts[0]) {
		unsigned char *text = texts[0];

		if (intl) text = _(text, term);
		*minwidth -= 4;
		*maxwidth -= 4;
		min_max_text_width(term, text, minwidth, maxwidth);
		*minwidth += 4;
		*maxwidth += 4;
		texts++;
	}
}


void
checkbox_list_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;

	min_max_checkboxes_width(term, 1, dlg->dlg->udata, &min, &max);
	buttons_width(term, dlg->items + dlg->n - 2, 2, &min, &max);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 5) w = 5;

	rw = 0;
	dlg_format_checkboxes(NULL, term, 1, dlg->items, dlg->n - 2, 0, &y, w,
			      &rw, dlg->dlg->udata);

	y++;
	dlg_format_buttons(NULL, term, dlg->items + dlg->n - 2, 2, 0, &y, w,
			   &rw, AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB + 1;
	dlg_format_checkboxes(term, term, 1, dlg->items, dlg->n - 2,
			      dlg->x + DIALOG_LB, &y, w, NULL,
			      dlg->dlg->udata);

	y++;
	dlg_format_buttons(term, term, dlg->items + dlg->n - 2, 2,
			   dlg->x + DIALOG_LB, &y, w, &rw,
			   AL_CENTER);
}


static void
display_checkbox(struct widget_data *di, struct dialog_data *dlg, int sel)
{
	struct terminal *term = dlg->win->term;

	if (di->checked) {
		print_text(term, di->x, di->y, 3,
				(!di->item->gid) ? "[X]" : "(X)",
				get_bfu_color(term, "dialog.checkbox"));
	} else {
		print_text(term, di->x,	di->y, 3,
				(!di->item->gid) ? "[ ]" : "( )",
				get_bfu_color(term, "dialog.checkbox"));
	}
	if (sel) {
		set_cursor(term, di->x + 1, di->y, di->x + 1,
				di->y);
		set_window_ptr(dlg->win, di->x, di->y);
	}
}

static void
init_checkbox(struct widget_data *widget, struct dialog_data *dialog,
	      struct event *ev)
{
	if (widget->item->gid) {
		if (*((int *) widget->cdata) == widget->item->gnum)
			widget->checked = 1;
	} else {
		if (*((int *) widget->cdata))
			widget->checked = 1;
	}
}

static int
mouse_checkbox(struct widget_data *di, struct dialog_data *dlg,
	       struct event *ev)
{
	if ((ev->b & BM_BUTT) >= B_WHEEL_UP
	    || ev->y != di->y || ev->x < di->x || ev->x >= di->x + 3)
		return EVENT_NOT_PROCESSED;
	display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
	dlg->selected = di - dlg->items;
	display_dlg_item(dlg, di, 1);
	if ((ev->b & BM_ACT) == B_UP && di->item->ops->select)
		di->item->ops->select(di, dlg);
	return EVENT_PROCESSED;
}

static void
select_checkbox(struct widget_data *di, struct dialog_data *dlg)
{
	if (!di->item->gid) {
		di->checked = *((int *) di->cdata)
			    = !*((int *) di->cdata);
	} else {
		int i;

		for (i = 0; i < dlg->n; i++) {
			if (dlg->items[i].item->type == D_CHECKBOX
			    && dlg->items[i].item->gid == di->item->gid) {
				*((int *) dlg->items[i].cdata) = di->item->gnum;
				dlg->items[i].checked = 0;
				display_dlg_item(dlg, &dlg->items[i], 0);
			}
		}
		di->checked = 1;
	}
	display_dlg_item(dlg, di, 1);
}

struct widget_ops checkbox_ops = {
	display_checkbox,
	init_checkbox,
	mouse_checkbox,
	NULL,
	select_checkbox,
};
