/* Listbox widget implementation. */
/* $Id: listbox.c,v 1.4 2002/07/05 20:42:13 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/colors.h"
#include "bfu/dialog.h"
#include "bfu/listbox.h"
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
void box_sel_set_visible(struct widget_data *box_item_data, int offset)
{
	struct dlg_data_item_data_box *box;
	int sel;

	box = (struct dlg_data_item_data_box *)(box_item_data->item->data);
	if (offset > box_item_data->item->gid || offset < 0) {
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
void box_sel_move(struct widget_data *box_item_data, int dist)
{
    struct dlg_data_item_data_box *box;
	int new_sel;
	int new_top;

	box = (struct dlg_data_item_data_box *)(box_item_data->item->data);

	new_sel = box->sel + dist;
	new_top = box->box_top;

	/* Ensure that the selection is in range */
	if (new_sel < 0)
		new_sel = 0;
	else if (new_sel >= box->list_len)
		new_sel = box->list_len - 1;

	/* Ensure that the display box is over the item */
	if (new_sel >= (new_top + box_item_data->item->gid)) {
		/* Move it down */
		new_top = new_sel - box_item_data->item->gid + 1;
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
display_listbox(struct widget_data *box_item_data, struct dialog_data *dlg,
		int sel)
{
	struct terminal *term = dlg->win->term;
	struct dlg_data_item_data_box *box;
	struct box_item *citem;	/* Item currently being shown */
	int n;	/* Index of item currently being displayed */

	box = (struct dlg_data_item_data_box *)(box_item_data->item->data);
	/* FIXME: Counting here SHOULD be unnecessary */
	n = 0;

	fill_area(term, box_item_data->x, box_item_data->y, box_item_data->l,
		  box_item_data->item->gid, COLOR_DIALOG_FIELD);

	foreach (citem, box->items) {
		int len; /* Length of the current text field. */

		len = strlen(citem->text);
		if (len > box_item_data->l) {
			len = box_item_data->l;
		}

		/* Is the current item in the region to be displayed? */
		if ((n >= box->box_top)
		    && (n < (box->box_top + box_item_data->item->gid))) {
			print_text(term, box_item_data->x,
				   box_item_data->y + n - box->box_top,
				   len, citem->text,
				   n == box->sel ? COLOR_DIALOG_BUTTON_SELECTED
						 : COLOR_DIALOG_FIELD_TEXT);
		}
		n++;
	}

	box->list_len = n;
}

struct widget_ops listbox_ops = {
	display_listbox,
};
