/* Common widget functions. */
/* $Id: widget.c,v 1.1 2002/07/04 21:04:45 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/bfu.h"
#include "bfu/listbox.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "util/error.h"


/* display_dlg_item() */
void display_dlg_item(struct dialog_data *dlg, struct widget_data *di,
		      int sel)
{
	struct terminal *term = dlg->win->term;

	switch(di->item->type) {
		int co;
		unsigned char *text;

		case D_CHECKBOX:
			if (di->checked) {
				print_text(term, di->x, di->y, 3,
					   (!di->item->gid) ? "[X]" : "(X)",
					   COLOR_DIALOG_CHECKBOX);
			} else {
				print_text(term, di->x,	di->y, 3,
					   (!di->item->gid) ? "[ ]" : "( )",
					   COLOR_DIALOG_CHECKBOX);
			}
			if (sel) {
				set_cursor(term, di->x + 1, di->y, di->x + 1,
					   di->y);
				set_window_ptr(dlg->win, di->x, di->y);
			}
			break;

		case D_FIELD_PASS:
		case D_FIELD:
			if (di->vpos + di->l <= di->cpos)
				di->vpos = di->cpos - di->l + 1;
			if (di->vpos > di->cpos)
				di->vpos = di->cpos;
			if (di->vpos < 0)
				di->vpos = 0;

			fill_area(term, di->x, di->y, di->l, 1,
				  COLOR_DIALOG_FIELD);
			{
				int len = strlen(di->cdata + di->vpos);

				if (di->item->type == D_FIELD) {
					print_text(term, di->x, di->y,
						   len <= di->l ? len : di->l,
						   di->cdata + di->vpos,
						   COLOR_DIALOG_FIELD_TEXT);
				} else {
					fill_area(term, di->x, di->y,
						  len <= di->l ? len : di->l, 1,
						  COLOR_DIALOG_FIELD_TEXT | '*');
				}
			}
			if (sel) {
				int x = di->x + di->cpos - di->vpos;

				set_cursor(term, x, di->y, x, di->y);
				set_window_ptr(dlg->win, di->x, di->y);
			}
			break;

		case D_BUTTON:
			co = sel ? COLOR_DIALOG_BUTTON_SELECTED
				 : COLOR_DIALOG_BUTTON;
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
			break;

		case D_BOX:
			/* Draw a hierarchy box */
			show_dlg_item_box(dlg, di);
			break;

		default:
			debug("Tried to draw unknown ");
	}
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
