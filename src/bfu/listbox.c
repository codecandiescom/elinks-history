/* Listbox widget implementation. */
/* $Id: listbox.c,v 1.47 2002/11/30 22:42:35 pasky Exp $ */

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
	/* We ignore this one happily now. Rather be Dynamic ;p. */
	/* (*y) += item->item->gid; */
	/* This is only weird heuristic, it could scale well I hope. */
	item->h = (term ? term->y : t2 ? t2->y : 25) * 2 / 3 - 2 * DIALOG_TB - 8;
	if (item->h < 6) item->h = 6;
	/* debug("::%d(%d)::%d::%d::", term?term->y:t2->y, term?1:2, item->h, *y); */
	(*y) += item->h;
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
 * optionally calling user function for each of the items visited (basically,
 * each of the items we move _through_, that means from the input item to the
 * item _before_ the output item). */
/* From the box structure, we should use only 'items' here. */
struct listbox_item *
traverse_listbox_items_list(struct listbox_item *item, int offset,
			    int follow_visible,
			    int (*fn)(struct listbox_item *, void *, int),
			    void *d)
{
	struct listbox_item *visible_item = item;
	struct listbox_data *box;
	int levmove = 0;
	int stop = 0;

	if (!item) return NULL;
	box = item->box->next;

	while (offset && !stop) {
		if (fn && (!follow_visible || item->visible))
			offset = fn(item, d, offset);

		if (offset > 0) {
			/* Otherwise we climb back up when last item in root
			 * is a folder. */
			struct listbox_item *cragsman = NULL;

			/* Direction DOWN. */

			offset--;

			if (!list_empty(item->child) && item->expanded
			    && (!follow_visible || item->visible)) {
				/* Descend to children. */
				item = item->child.next;
				visible_item = item;
				continue;
			}

			while (item->root
			       && (void *) item->next == &item->root->child) {
				/* Last item in a non-root list, climb to your
				 * root. */
				if (!cragsman) cragsman = item;
				item = item->root;
			}

			if (!item->root && (void *) item->next == box->items) {
				/* Last item in the root list, quit.. */
				stop = 1;
				if (cragsman) {
					/* ..and fall back where we were. */
					item = cragsman;
				}
			}

			/* We're not at the end of anything, go on. */
			if (!stop) item = item->next;

			if (follow_visible && !item->visible) {
				offset++;
			} else {
				visible_item = item;
			}

		} else {
			/* Direction UP. */

			offset++;

			if (item->root
			    && (void *) item->prev == &item->root->child) {
				/* First item in a non-root list, climb to your
				 * root. */
				item = item->root;
				levmove = 1;
			}

			if (!item->root && (void *) item->prev == box->items) {
				/* First item in the root list, quit. */
				stop = 1;
				levmove = 1;
			}

			/* We're not at the start of anything, go on. */
			if (!levmove) {
				if (!stop) item = item->prev;

				while (!list_empty(item->child) && item->expanded
					&& (!follow_visible || item->visible)) {
					/* Descend to children. */
					item = item->child.prev;
					visible_item = item;
				}
			} else {
				levmove = 0;
			}

			if (follow_visible && !item->visible) {
				offset--;
			} else {
				visible_item = item;
			}
		}
	}

	return visible_item;
}

struct box_context {
	struct terminal *term;
	struct widget_data *listbox_item_data;
	struct listbox_data *box;
	int dist;
	int offset;
};

/* Takes care about listbox top moving. */
int
box_sel_move_do(struct listbox_item *item, void *data_, int offset)
{
	struct box_context *data = data_;

	if (item == data->box->top)
		data->box->sel_offset = 0; /* assure resync */

	if (data->dist > 0) {
		if (data->box->sel_offset
		    < data->listbox_item_data->h - 1) {
			data->box->sel_offset++;
		} else {
			data->box->top =
				traverse_listbox_items_list(data->box->top,
					1, 1, NULL, NULL);
		}
	} else if (data->dist < 0) {
		if (data->box->sel_offset > 0) {
			data->box->sel_offset--;
		} else {
			data->box->top =
				traverse_listbox_items_list(data->box->top,
					-1, 1, NULL, NULL);
		}
	}

	return offset;
}

/* Moves the selected item by [dist] items. If [dist] is out of the current
 * range, the selected item is moved to the extreme (ie, the top or bottom) */
void
box_sel_move(struct widget_data *listbox_item_data, int dist)
{
	struct listbox_data *box;
	struct box_context *data;

	box = (struct listbox_data *) listbox_item_data->item->data;
	if (!list_empty(*box->items)) {
		if (!box->top) box->top = box->items->next;
		if (!box->sel) box->sel = box->top;
	}

	data = mem_alloc(sizeof(struct box_context));
	if (!data) return;
	data->box = box;
	data->listbox_item_data = listbox_item_data;
	data->dist = dist;

	/* We want to have these visible if possible. */
	if (box->top && !box->top->visible) {
		box->top = traverse_listbox_items_list(box->top,
				1, 1, NULL, NULL);
		box->sel = box->top;
	}

	if (traverse_listbox_items_list(box->sel, dist, 1, NULL, NULL)
	    != box->sel) {
		/* XXX: This is ugly, yes; but we don't want to call the
		 * callback if we won't move on at all. */
		box->sel = traverse_listbox_items_list(box->sel, dist, 1,
						       box_sel_move_do, data);
	}

	mem_free(data);
}


/* Takes care about rendering of each listbox item. */
int
display_listbox_item(struct listbox_item *item, void *data_, int offset)
{
	struct box_context *data = data_;
	int len; /* Length of the current text field. */
	int color;
	int depth = item->depth + 1;
	int d;

	len = strlen(item->text);
	if (len > data->listbox_item_data->l - depth * 5) {
		len = data->listbox_item_data->l - depth * 5;
	}

	if (item == data->box->sel) {
		color = get_bfu_color(data->term, "menu.selected");
	} else {
		color = get_bfu_color(data->term, "menu.normal");
	}

	/* TODO: Use graphics chars for lines if available. --pasky */

	for (d = 0; d < depth - 1; d++) {
		struct listbox_item *root = item;
		struct listbox_item *child = item;
		int i;
		unsigned char str[6] = " |   ";

		for (i = depth - d; i; i--) {
			child = root;
			root = root->root;
		}

		if (root ? root->child.prev == child
			 : data->box->items->prev == child)
			strcpy(str, "     "); /* We were the last branch. */

		/* TODO: Don't draw this if there's no further child of
		 * parent in that depth! Links suffers with the same, it
		 * looks sooo ugly ;-). --pasky */
		print_text(data->term, data->listbox_item_data->x + d * 5,
			   data->listbox_item_data->y + data->offset,
			   5, str, color);
	}

	if (depth) {
		unsigned char str[6] = " |-- ";

		if (item->type == BI_LEAF) {
			if (item->root) {
				if (item == item->root->child.prev) {
					str[1] = '`';
				}
			} else {
				if (((struct listbox_data *) item->box->next)->items->next == item) {
					str[1] = ',';
				} else if (((struct listbox_data *) item->box->next)->items->prev == item) {
					str[1] = '`';
				}
			}
		} else {
			if (item->expanded) {
				strcpy(str, "[-]- ");
			} else {
				strcpy(str, "[+]- ");
			}
		}

		if (item->marked) str[4] = '*';

		print_text(data->term,
			   data->listbox_item_data->x + (depth - 1) * 5,
			   data->listbox_item_data->y + data->offset,
			   5, str, color);
	}

	print_text(data->term, data->listbox_item_data->x + depth * 5,
		   data->listbox_item_data->y + data->offset,
		   len, item->text, color);

	data->offset++;

	return offset;
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
	if (!list_empty(*box->items)) {
		if (!box->top) box->top = box->items->next;
		if (!box->sel) box->sel = box->top;
	}

	fill_area(term, listbox_item_data->x, listbox_item_data->y,
		  listbox_item_data->l, listbox_item_data->h,
		  get_bfu_color(term, "menu.normal"));

	data = mem_alloc(sizeof(struct box_context));
	if (!data) return;
	data->term = term;
	data->listbox_item_data = listbox_item_data;
	data->box = box;
	data->offset = 0;

	/* We want to have these visible if possible. */
	if (box->top && !box->top->visible) {
/*		debug("top: %s - (%d) %p\n", box->top->text, box->top->visible, box->top); */
		box->top = traverse_listbox_items_list(box->top, 1,
				1, NULL, NULL);
		box->sel = box->top;
	}

	traverse_listbox_items_list(box->top, listbox_item_data->h,
				    1, display_listbox_item, data);

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
	if (!list_empty(*box->items)) {
		if (!box->top) box->top = box->items->next;
		if (!box->sel) box->sel = box->top;
	}

	if ((ev->b & BM_ACT) == B_DOWN)
		switch (ev->b & BM_BUTT) {
			case B_WHEEL_DOWN:
				box_sel_move(&dlg->items[dlg->n - 1], 1);
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);
				return EVENT_PROCESSED;

			case B_WHEEL_UP:
				box_sel_move(&dlg->items[dlg->n - 1], -1);
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);
				return EVENT_PROCESSED;
		}

	if ((ev->b & BM_ACT) == B_UP) {
		if ((ev->b & BM_BUTT) < B_WHEEL_UP &&
		    (ev->y >= di->y && ev->y < di->y + di->h) &&
		    (ev->x >= di->x && ev->x <= di->x + di->l)) {
			/* Clicked in the box. */
			int offset;

			offset = ev->y - di->y;
			box->sel_offset = offset;
			box->sel = traverse_listbox_items_list(box->top,
							       offset, 1,
							       NULL, NULL);
			display_dlg_item(dlg, di, 1);
			return EVENT_PROCESSED;
		}
	}
#if 0
	else if (ev->b & BM_DRAG) {
		debug("drag");
	}
#endif
	return EVENT_NOT_PROCESSED;
}

int
kbd_listbox(struct widget_data *di, struct dialog_data *dlg, struct event *ev)
{
	/* Not a pure listbox, but you're not supposed to use this outside of
	 * the listbox browser anyway, so what.. */
	/* We rely ie. on the fact that listbox is the last item of the dialog
	 * and so on; we definitively shouldn't, but handling of all the stuff
	 * would be much more painful. */

	switch (ev->ev) {
		case EV_KBD:
			/* Catch change focus requests */
			if (ev->x == KBD_RIGHT || (ev->x == KBD_TAB && !ev->y)) {
				/* Move right */
				display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
				if (++dlg->selected >= dlg->n - 1)
					dlg->selected = 0;
				display_dlg_item(dlg, &dlg->items[dlg->selected], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_LEFT || (ev->x == KBD_TAB && ev->y)) {
				/* Move left */
				display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
				if (--dlg->selected < 0)
					dlg->selected = dlg->n - 2;
				display_dlg_item(dlg, &dlg->items[dlg->selected], 1);

				return EVENT_PROCESSED;
			}

			/* Moving the box */
			if (ev->x == KBD_DOWN) {
				box_sel_move(&dlg->items[dlg->n - 1], 1);
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_UP) {
				box_sel_move(&dlg->items[dlg->n - 1], -1);
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_DOWN) {
				box_sel_move(&dlg->items[dlg->n - 1],
					     dlg->items[dlg->n - 1].h / 2);
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_UP) {
				box_sel_move(&dlg->items[dlg->n - 1],
					     -dlg->items[dlg->n - 1].h / 2);
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_HOME) {
				box_sel_move(&dlg->items[dlg->n - 1], -MAXINT);
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_END) {
				box_sel_move(&dlg->items[dlg->n - 1], MAXINT);
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_INS) {
				struct listbox_data *box;

				box = (struct listbox_data *) dlg->items[dlg->n - 1].item->data;
				if (box->sel) {
					box->sel->marked = !box->sel->marked;
					box_sel_move(&dlg->items[dlg->n - 1], 1);
				}
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);

				return EVENT_PROCESSED;
			}

			/* Selecting a button; most probably ;). */
			break;

		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
		case EV_MOUSE:
		case EV_ABORT:
			break;

		default:
			internal("Unknown event received: %d", ev->ev);
	}

	return EVENT_NOT_PROCESSED;
}

struct widget_ops listbox_ops = {
	display_listbox,
	init_listbox,
	mouse_listbox,
	kbd_listbox,
	NULL,
};
