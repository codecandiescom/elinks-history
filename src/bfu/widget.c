/* Common widget functions. */
/* $Id: widget.c,v 1.4 2002/07/09 15:21:38 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "util/error.h"


void
display_dlg_item(struct dialog_data *dialog, struct widget_data *widget,
		 int selected)
{
	if (widget->item->ops->display)
		widget->item->ops->display(widget, dialog, selected);
}

/* dlg_select_item() */
void dlg_select_item(struct dialog_data *dlg, struct widget_data *di)
{
	if (di->item->type == D_CHECKBOX) {
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

	} else {
		if (di->item->type == D_BUTTON)
			di->item->fn(dlg, di);
	}
}

/* dlg_set_history() */
void dlg_set_history(struct widget_data *di)
{
	unsigned char *s = "";
	int len;

	if ((void *) di->cur_hist != &di->history)
		s = di->cur_hist->d;
	len = strlen(s);
	if (len > di->item->dlen)
		len = di->item->dlen - 1;
	memcpy(di->cdata, s, len);
	di->cdata[len] = 0;
	di->cpos = len;
}

/* dlg_mouse() */
int dlg_mouse(struct dialog_data *dlg, struct widget_data *di,
	      struct event *ev)
{
	switch (di->item->type) {
		case D_BUTTON:
			if (ev->y != di->y || ev->x < di->x
			    || ev->x >= di->x + strlen(_(di->item->text,
						       dlg->win->term)) + 4)
			   	return 0;

			display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
			dlg->selected = di - dlg->items;
			display_dlg_item(dlg, di, 1);
			if ((ev->b & BM_ACT) == B_UP)
				dlg_select_item(dlg, di);
			return 1;

		case D_FIELD_PASS:
		case D_FIELD:
			if (ev->y != di->y || ev->x < di->x
			    || ev->x >= di->x + di->l)
				return 0;
			di->cpos = di->vpos + ev->x - di->x;
			{
				int len = strlen(di->cdata);

				if (di->cpos > len)
					di->cpos = len;
			}
			display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
			dlg->selected = di - dlg->items;
			display_dlg_item(dlg, di, 1);
			return 1;

		case D_CHECKBOX:
			if (ev->y != di->y || ev->x < di->x
			    || ev->x >= di->x + 3)
				return 0;
			display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
			dlg->selected = di - dlg->items;
			display_dlg_item(dlg, di, 1);
			if ((ev->b & BM_ACT) == B_UP)
				dlg_select_item(dlg, di);
			return 1;

		case D_BOX:
			if ((ev->b & BM_ACT) == B_UP) {
				if ((ev->y >= di->y)
				    && (ev->x >= di->x &&
					ev->x <= di->l + di->x)) {
					/* Clicked in the box. */
					int offset;

					offset = ev->y - di->y;
					box_sel_set_visible(di, offset);
					display_dlg_item(dlg, di, 1);
					return 1;
				}
			}
#if 0
			else if ((ev->b & BM_ACT) == B_DRAG) {
					debug("drag");
			}
#endif
		case D_END:
			/* Silence compiler warnings */
			break;
	}

	return 0;
}
