/* Listbox widget implementation. */
/* $Id: listbox.c,v 1.12 2002/08/11 18:25:48 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"


/* Layout for generic boxes */
void dlg_format_box(struct terminal *term, struct terminal *t2,
		    struct widget_data *item,
		    int x, int *y, int w, int *rw, enum format_align align)
{
	item->x = x;
	item->y = *y;
	item->l = w;

	if (rw && item->l > *rw) {
		*rw = item->l;
		if (*rw > w) *rw = w;
	}
	(*y) += item->item->gid;
}


/* Sets the selected item to one that is visible.*/
void box_sel_set_visible(struct widget_data *listbox_item_data, int offset)
{
	struct listbox_data *box;
	int sel;

	box = (struct listbox_data *)(listbox_item_data->item->data);
	if (offset > listbox_item_data->item->gid || offset < 0) {
		return;
	}

	/* debug("offset: %d", offset); */
	sel = box->box_top + offset;

	if (sel > box->list_len) {
		box->sel = box->list_len - 1;
	} else {
		box->sel = sel;
	}
}

/* Moves the selected item [dist] thingies. If [dist] is out of the current
 * range, the selected item is moved to the extreme (ie, the top or bottom) */
void box_sel_move(struct widget_data *listbox_item_data, int dist)
{
    struct listbox_data *box;
	int new_sel;
	int new_top;

	box = (struct listbox_data *)(listbox_item_data->item->data);

	new_sel = box->sel + dist;
	new_top = box->box_top;

	/* Ensure that the selection is in range */
	if (new_sel < 0)
		new_sel = 0;
	else if (new_sel >= box->list_len)
		new_sel = box->list_len - 1;

	/* Ensure that the display box is over the item */
	if (new_sel >= (new_top + listbox_item_data->item->gid)) {
		/* Move it down */
		new_top = new_sel - listbox_item_data->item->gid + 1;
#ifdef DEBUG
		if (new_top < 0)
			debug("Newly calculated box_top is an extremely wrong value (%d). It should not be below zero.", new_top);
#endif
	} else if (new_sel < new_top) {
		/* Move the display up (if necessary) */
		new_top = new_sel;
	}

	box->sel = new_sel;
	box->box_top = new_top;
}


/* Displays a dialog box */
void
display_listbox(struct widget_data *listbox_item_data, struct dialog_data *dlg,
		int sel)
{
	struct terminal *term = dlg->win->term;
	struct listbox_data *box;
	struct listbox_item *citem;	/* Item currently being shown */
	int n;	/* Index of item currently being displayed */

	box = (struct listbox_data *)(listbox_item_data->item->data);
	/* FIXME: Counting here SHOULD be unnecessary */
	n = 0;

	fill_area(term, listbox_item_data->x, listbox_item_data->y, listbox_item_data->l,
		  listbox_item_data->item->gid, get_bfu_color(term, "menu.normal"));

	foreach (citem, box->items) {
		int len; /* Length of the current text field. */

		len = strlen(citem->text);
		if (len > listbox_item_data->l) {
			len = listbox_item_data->l;
		}

		/* Is the current item in the region to be displayed? */
		if ((n >= box->box_top)
		    && (n < (box->box_top + listbox_item_data->item->gid))) {
			print_text(term, listbox_item_data->x,
				   listbox_item_data->y + n - box->box_top,
				   len, citem->text,
				   n == box->sel ? get_bfu_color(term, "menu.selected")
						 : get_bfu_color(term, "menu.normal"));
		}
		n++;
	}

	box->list_len = n;
}

void
init_listbox(struct widget_data *widget, struct dialog_data *dialog,
	     struct event *ev)
{
	/* Freed in bookmark_dialog_abort_handler() */
	widget->cdata = mem_alloc(sizeof(struct listbox_data));
	if (!widget->cdata)
		return;

	((struct listbox_data *) widget->cdata)->sel = -1;
	((struct listbox_data *) widget->cdata)->box_top = 0;
	((struct listbox_data *) widget->cdata)->list_len = -1;

	init_list(((struct listbox_data*) widget->cdata)->items);
}

int
mouse_listbox(struct widget_data *di, struct dialog_data *dlg,
	      struct event *ev)
{
	if ((ev->b & BM_ACT) == B_UP) {
		if ((ev->y >= di->y) && (ev->x >= di->x &&
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
	return 0;
}

struct widget_ops listbox_ops = {
	display_listbox,
	init_listbox,
	mouse_listbox,
};
