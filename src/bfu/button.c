/* Button widget handlers. */
/* $Id: button.c,v 1.7 2002/08/07 02:56:58 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/align.h"
#include "bfu/colors.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"


/* max_buttons_width() */
void max_buttons_width(struct terminal *term, struct widget_data *butt,
		       int n, int *width)
{
	int w = -2;
	int i;

	for (i = 0; i < n; i++)
		w += strlen(_((butt++)->item->text, term)) + 6;
	if (w > *width) *width = w;
}

/* min_buttons_width() */
void min_buttons_width(struct terminal *term, struct widget_data *butt,
		       int n, int *width)
{
	int i;

	for (i = 0; i < n; i++) {
		int w = strlen(_((butt++)->item->text, term)) + 4;

		if (w > *width) *width = w;
	}
}

/* dlg_format_buttons() */
void dlg_format_buttons(struct terminal *term, struct terminal *t2,
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
			int p = x + ((align & AL_MASK) == AL_CENTER ? (w - mw) / 2 : 0);

			for (i = i1; i < i2; i++) {
				butt[i].x = p;
				butt[i].y = *y;
				butt[i].l = strlen(_(butt[i].item->text, t2)) + 4;
				p += butt[i].l + 2;
			}
		}

		*y += 2;
		i1 = i2;
	}
}


void
display_button(struct widget_data *di, struct dialog_data *dlg, int sel)
{
	struct terminal *term = dlg->win->term;
	int co;
	unsigned char *text;

	co = sel ? get_bfu_color(term, "dialog.button-selected")
		: get_bfu_color(term, "dialog.button");
	text = _(di->item->text, term);
	{
		int len = strlen(text);
		int x = di->x + 2;

		print_text(term, di->x, di->y, 2, "[ ", co);
		print_text(term, x, di->y, len, text, co);
		print_text(term, x + len, di->y, 2, " ]", co);
		if (sel) {
			set_cursor(term, x, di->y, x, di->y);
			set_window_ptr(dlg->win, di->x, di->y);
		}
	}
}

int
mouse_button(struct widget_data *di, struct dialog_data *dlg, struct event *ev)
{
	if (ev->y != di->y || ev->x < di->x
	    || ev->x >= di->x + strlen(_(di->item->text, dlg->win->term)) + 4)
		return 0;

	display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
	dlg->selected = di - dlg->items;
	display_dlg_item(dlg, di, 1);
	if ((ev->b & BM_ACT) == B_UP)
		dlg_select_item(dlg, di);
	return 1;
}

struct widget_ops button_ops = {
	display_button,
	NULL,
	mouse_button,
};

