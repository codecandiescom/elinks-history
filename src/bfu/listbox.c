/* Listbox widget implementation. */
/* $Id: listbox.c,v 1.16 2002/08/29 11:48:30 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "util/lists.h"


/* Layout for generic boxes */
void
dlg_format_box(struct terminal *term, struct terminal *t2,
	       struct widget_data *item, int x, int *y, int w, int *rw,
	       enum format_align align)
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


/*
 *,item00->prev
 *|item00->root = NULL  ,item10->prev
 *|item00->child <----->|item10->root
 *|item00->next <-.     |item10->child [<->]
 *|               |     `item10->next
 *|item01->prev <-'
 *|item01->root = NULL
 *|item01->child [<->]
 *|item01->next <-.
 *|               |
 *|item02->prev <-'
 *|item02->root = NULL  ,item11->prev
 *|item02->child <----->|item11->root         ,item20->prev
 *`item02->next       \ |item11->child <----->|item20->root
 *                    | |item11->next <-.     |item20->child [<->]
 *                    | |               |     `item20->next
 *                    | |item12->prev <-'
 *                    `-|item12->root
 *                    | |item12->child [<->]
 *                    | |item12->next <-.
 *                    | |               |
 *                    | |item13->prev <-'
 *                    `-|item13->root         ,item21->prev
 *                      |item13->child <----->|item21->root
 *                      `item13->next         |item21->child [<->]
 *                                            `item21->next
 *
 */

/* Traverse through the hiearchic tree in specified direction by N items,
 * optionally calling user function for each of the items visited. */
/* From the box structure, we should use only 'items' here. */
struct listbox_item *
traverse_listbox_items_list(struct listbox_item *item, int offset,
			    void (*fn)(struct listbox_item *, void *), void *d)
{
	struct listbox_data *box;

	if (!item) return NULL;

	box = (struct listbox_data *) item->data;

	while (offset) {
		if (offset > 0) {
			/* Direction UP. */

			offset--;

			if (!list_empty(item->child) && item->expanded) {
				/* Descend to children. */
				item = item->child.next;
				goto next;
			}

			while (item->root
			       && (void *) item->next == &item->root->child) {
				/* Last item in a non-root list, climb to your
				 * root. */
				item = item->root;
			}

			if (!item->root && (void *) item->next == box->items) {
				/* Last item in the root list, quit. */
				break;
			}

			/* We're not at the end of anything, go on. */
			item = item->next;

		} else {
			/* Direction DOWN. */

			offset++;

			if (!list_empty(item->child) && item->expanded) {
				/* Descend to children. */
				item = item->child.prev;
				goto next;
			}

			while (item->root
			       && (void *) item->prev == &item->root->child) {
				/* First item in a non-root list, climb to your
				 * root. */
				item = item->root;
			}

			if (!item->root && (void *) item->prev == box->items) {
				/* First item in the root list, quit. */
				break;
			}

			/* We're not at the start of anything, go on. */
			item = item->prev;
		}

next:
		if (fn) fn (item, d);
	}

	return item;
}

struct box_context {
	struct terminal *term;
	struct widget_data *listbox_item_data;
	struct listbox_data *box;
	struct listbox_item *new_top;
	int dist;
	int offset;
};

/* Takes care about listbox top moving. */
void
box_sel_move_do(struct listbox_item *item, void *data_)
{
	struct box_context *data = data_;

	data->offset += (data->dist > 0) ? 1 : -1;
	if (data->offset < 0) {
		data->new_top = item;
	} else if (data->offset > data->listbox_item_data->item->gid) {
		data->new_top = traverse_listbox_items_list(data->new_top, 1,
							    NULL, NULL);
	}
}

/* Moves the selected item by [dist] items. If [dist] is out of the current
 * range, the selected item is moved to the extreme (ie, the top or bottom) */
void
box_sel_move(struct widget_data *listbox_item_data, int dist)
{
	struct listbox_data *box;
	struct box_context *data;

	box = (struct listbox_data *) listbox_item_data->item->data;

	data = mem_alloc(sizeof(struct box_context));
	data->new_top = box->top;
	data->listbox_item_data = listbox_item_data;
	data->dist = dist;
	data->offset = 0;

	box->sel = traverse_listbox_items_list(box->top, dist,
					       box_sel_move_do, data);

	box->top = data->new_top;
	mem_free(data);
}


/* Takes care about rendering of each listbox item. */
void
display_listbox_item(struct listbox_item *item, void *data_)
{
	struct box_context *data = data_;
	int len; /* Length of the current text field. */
	int color;

	len = strlen(item->text);
	if (len > data->listbox_item_data->l) {
		len = data->listbox_item_data->l;
	}

	if (item == data->box->sel) {
		color = get_bfu_color(data->term, "menu.selected");
	} else {
		color = get_bfu_color(data->term, "menu.normal");
	}

	print_text(data->term, data->listbox_item_data->x,
		   data->listbox_item_data->y + data->offset,
		   len, item->text, color);

	data->offset++;
}

/* Displays a dialog box */
void
display_listbox(struct widget_data *listbox_item_data, struct dialog_data *dlg,
		int sel)
{
	struct terminal *term = dlg->win->term;
	struct listbox_data *box;
	struct box_context *data;

	box = (struct listbox_data *) listbox_item_data->item->data;

	fill_area(term, listbox_item_data->x, listbox_item_data->y,
		  listbox_item_data->l, listbox_item_data->item->gid,
		  get_bfu_color(term, "menu.normal"));

	data = mem_alloc(sizeof(struct box_context));
	data->term = term;
	data->listbox_item_data = listbox_item_data;
	data->box = box;
	data->offset = 0;

	traverse_listbox_items_list(box->top, listbox_item_data->item->gid,
				    display_listbox_item, data);

	mem_free(data);
}

void
init_listbox(struct widget_data *widget, struct dialog_data *dialog,
	     struct event *ev)
{
#if 0
	/* Freed in bookmark_dialog_abort_handler() */
	widget->cdata = mem_alloc(sizeof(struct listbox_data));
	if (!widget->cdata)
		return;

	((struct listbox_data *) widget->cdata)->sel = NULL;
	((struct listbox_data *) widget->cdata)->top = NULL;

	((struct listbox_data *) widget->cdata)->items =
		mem_alloc(sizeof(struct list_head));
	init_list(*((struct listbox_data *) widget->cdata)->items);
#endif
}

int
mouse_listbox(struct widget_data *di, struct dialog_data *dlg,
	      struct event *ev)
{
	struct listbox_data *box;

	box = (struct listbox_data *) di->item->data;

	if ((ev->b & BM_ACT) == B_UP) {
		if ((ev->y >= di->y) && (ev->x >= di->x &&
					 ev->x <= di->l + di->x)) {
			/* Clicked in the box. */
			int offset;

			offset = ev->y - di->y;
			box->sel = traverse_listbox_items_list(box->top,
							       offset,
							       NULL, NULL);
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
