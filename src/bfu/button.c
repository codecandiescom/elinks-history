/* Button widget handlers. */
/* $Id: button.c,v 1.25 2003/06/27 20:39:32 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"


void
buttons_width(struct terminal *term, struct widget_data *butt,
		      int n, int *minwidth, int *maxwidth)
{
	int maxw = -2;
	register int i;

	for (i = 0; i < n; i++) {
		int minw = strlen((butt++)->item->text) + 4;

		maxw += minw + 2;
		if (minw > *minwidth) *minwidth = minw;
	}

	if (maxw > *maxwidth) *maxwidth = maxw;
}

void
max_buttons_width(struct terminal *term, struct widget_data *butt,
		  int n, int *width)
{
	int w = -2;
	int i;

	for (i = 0; i < n; i++)
		w += strlen((butt++)->item->text) + 6;
	if (w > *width) *width = w;
}

void
min_buttons_width(struct terminal *term, struct widget_data *butt,
		  int n, int *width)
{
	int i;

	for (i = 0; i < n; i++) {
		int w = strlen((butt++)->item->text) + 4;

		if (w > *width) *width = w;
	}
}

void
dlg_format_buttons(struct terminal *term, struct terminal *t2,
		   struct widget_data *butt, int n,
		   int x, int *y, int w, int *rw, enum format_align align)
{
	int i1 = 0;

	while (i1 < n) {
		int i2 = i1 + 1;
		int mw;

		while (i2 < n) {
			mw = 0;
			max_buttons_width(t2, butt + i1, i2 - i1 + 1, &mw);
			if (mw <= w) i2++;
			else break;
		}

		mw = 0;
		max_buttons_width(t2, butt + i1, i2 - i1, &mw);
		if (rw && mw > *rw) {
			*rw = mw;
			if (*rw > w) *rw = w;
		}

		if (term) {
			int i;
			int p = x + (align == AL_CENTER ? (w - mw) / 2 : 0);

			for (i = i1; i < i2; i++) {
				butt[i].x = p;
				butt[i].y = *y;
				butt[i].l = strlen(butt[i].item->text) + 4;
				p += butt[i].l + 2;
			}
		}

		*y += 2;
		i1 = i2;
	}
}


static void
display_button(struct widget_data *di, struct dialog_data *dlg, int sel)
{
	struct terminal *term = dlg->win->term;
	int co = sel ? get_bfu_color(term, "dialog.button-selected")
		     : get_bfu_color(term, "dialog.button");
	int len = strlen(di->item->text);
	int x = di->x + 2;

	print_text(term, di->x, di->y, 2, "[ ", co);
	print_text(term, x, di->y, len, di->item->text, co);
	print_text(term, x + len, di->y, 2, " ]", co);
	if (sel) {
		set_cursor(term, x, di->y, x, di->y);
		set_window_ptr(dlg->win, di->x, di->y);
	}
}

static int
mouse_button(struct widget_data *di, struct dialog_data *dlg, struct event *ev)
{
	if ((ev->b & BM_BUTT) >= B_WHEEL_UP || ev->y != di->y || ev->x < di->x
	    || ev->x >= di->x + strlen(di->item->text) + 4)
		return EVENT_NOT_PROCESSED;

	display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
	dlg->selected = di - dlg->items;
	display_dlg_item(dlg, di, 1);
	if ((ev->b & BM_ACT) == B_UP && di->item->ops->select)
		di->item->ops->select(di, dlg);
	return EVENT_PROCESSED;
}

static void
select_button(struct widget_data *di, struct dialog_data *dlg)
{
	di->item->fn(dlg, di);
}

struct widget_ops button_ops = {
	display_button,
	NULL,
	mouse_button,
	NULL,
	select_button,
};
