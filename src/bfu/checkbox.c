/* Checkbox widget handlers. */
/* $Id: checkbox.c,v 1.11 2002/09/10 11:13:32 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "links.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/text.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"


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
dlg_format_checkboxes(struct terminal *term, struct terminal *t2,
		      struct widget_data *chkb, int n,
		      int x, int *y, int w, int *rw,
		      unsigned char **texts)
{
	while (n) {
		dlg_format_checkbox(term, t2, chkb, x, y, w, rw,
				    _(texts[0], t2));
		texts++;
		chkb++;
		n--;
	}
}

void
checkboxes_width(struct terminal *term, unsigned char **texts, int *w,
		 void (*fn)(struct terminal *, unsigned char *, int *))
{
	while (texts[0]) {
		*w -= 4;
		fn(term, _(texts[0], term), w);
		*w += 4;
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

	checkboxes_width(term, dlg->dlg->udata, &max, max_text_width);
	checkboxes_width(term, dlg->dlg->udata, &min, min_text_width);
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 5) w = 5;

	rw = 0;
	dlg_format_checkboxes(NULL, term, dlg->items, dlg->n - 2, 0, &y, w,
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
	dlg_format_checkboxes(term, term, dlg->items, dlg->n - 2,
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

int
mouse_checkbox(struct widget_data *di, struct dialog_data *dlg,
	       struct event *ev)
{
	if (ev->y != di->y || ev->x < di->x
	    || ev->x >= di->x + 3)
		return 0;
	display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
	dlg->selected = di - dlg->items;
	display_dlg_item(dlg, di, 1);
	if ((ev->b & BM_ACT) == B_UP)
		dlg_select_item(dlg, di);
	return 1;
}

struct widget_ops checkbox_ops = {
	display_checkbox,
	init_checkbox,
	mouse_checkbox,
};
