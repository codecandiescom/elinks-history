/* Checkbox widget handlers. */
/* $Id: checkbox.c,v 1.46 2003/10/26 12:52:32 zas Exp $ */

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


static void
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
checkboxes_width(struct terminal *term, int intl, unsigned char **texts,
		 int *minwidth, int *maxwidth)
{
	*minwidth -= 4;
	*maxwidth -= 4;

	while (texts[0]) {
		unsigned char *text = texts[0];

		if (intl) text = _(text, term);
		text_width(term, text, minwidth, maxwidth);
		texts++;
	}

	*minwidth += 4;
	*maxwidth += 4;
}


void
checkbox_list_fn(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	int n = dlg_data->n - 2;

	checkboxes_width(term, 1, dlg_data->dlg->udata, &min, &max);
	buttons_width(term, dlg_data->items + n, 2, &min, &max);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	int_bounds(&w, min, max);
	int_bounds(&w, 5, term->x - 2 * DIALOG_LB);

	rw = 0;
	dlg_format_checkboxes(NULL, term, 1, dlg_data->items, n, 0, &y, w,
			      &rw, dlg_data->dlg->udata);

	y++;
	dlg_format_buttons(NULL, term, dlg_data->items + n, 2, 0, &y, w,
			   &rw, AL_CENTER);

	w = rw;
	dlg_data->xw = rw + 2 * DIALOG_LB;
	dlg_data->yw = y + 2 * DIALOG_TB;

	center_dlg(dlg_data);
	draw_dlg(dlg_data);

	y = dlg_data->y + DIALOG_TB + 1;
	dlg_format_checkboxes(term, term, 1, dlg_data->items, n,
			      dlg_data->x + DIALOG_LB, &y, w, NULL,
			      dlg_data->dlg->udata);

	y++;
	dlg_format_buttons(term, term, dlg_data->items + n, 2,
			   dlg_data->x + DIALOG_LB, &y, w, &rw,
			   AL_CENTER);
}


static void
display_checkbox(struct widget_data *di, struct dialog_data *dlg_data, int sel)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color;
	unsigned char *text;

	color = get_bfu_color(term, "dialog.checkbox");
	if (!color) return;

	if (di->checked) {
		text = (!di->widget->gid) ? "[X]" : "(X)";
	} else {
		text = (!di->widget->gid) ? "[ ]" : "( )";
	}

	draw_text(term, di->x,	di->y, text, 3, 0, color);

	if (sel) {
		set_cursor(term, di->x + 1, di->y, 0);
		set_window_ptr(dlg_data->win, di->x, di->y);
	}
}

static void
init_checkbox(struct widget_data *widget, struct dialog_data *dlg_data,
	      struct term_event *ev)
{
	if (widget->widget->gid) {
		if (*((int *) widget->cdata) == widget->widget->gnum)
			widget->checked = 1;
	} else {
		if (*((int *) widget->cdata))
			widget->checked = 1;
	}
}

static int
mouse_checkbox(struct widget_data *di, struct dialog_data *dlg_data,
	       struct term_event *ev)
{
	if ((ev->b & BM_BUTT) >= B_WHEEL_UP
	    || ev->y != di->y || ev->x < di->x || ev->x >= di->x + 3)
		return EVENT_NOT_PROCESSED;
	display_dlg_item(dlg_data, selected_widget(dlg_data), 0);
	dlg_data->selected = di - dlg_data->items;
	display_dlg_item(dlg_data, di, 1);
	if ((ev->b & BM_ACT) == B_UP && di->widget->ops->select)
		di->widget->ops->select(di, dlg_data);
	return EVENT_PROCESSED;
}

static void
select_checkbox(struct widget_data *di, struct dialog_data *dlg_data)
{
	if (!di->widget->gid) {
		di->checked = *((int *) di->cdata)
			    = !*((int *) di->cdata);
	} else {
		int i;

		for (i = 0; i < dlg_data->n; i++) {
			if (dlg_data->items[i].widget->type != D_CHECKBOX
			    || dlg_data->items[i].widget->gid != di->widget->gid)
				continue;

			*((int *) dlg_data->items[i].cdata) = di->widget->gnum;
			dlg_data->items[i].checked = 0;
			display_dlg_item(dlg_data, &dlg_data->items[i], 0);
		}
		di->checked = 1;
	}
	display_dlg_item(dlg_data, di, 1);
}

struct widget_ops checkbox_ops = {
	display_checkbox,
	init_checkbox,
	mouse_checkbox,
	NULL,
	select_checkbox,
};
