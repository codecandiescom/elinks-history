/* Listbox widget implementation. */
/* $Id: listbox.c,v 1.73 2003/05/04 17:25:51 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "intl/gettext/libintl.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/lists.h"


/* Layout for generic boxes */
void
dlg_format_box(struct terminal *term, struct terminal *t2,
	       struct widget_data *item, int x, int *y, int w, int *rw,
	       enum format_align align)
{
	int min, optimal_h, max_y = term ? term->y : t2 ? t2->y : 25;

	item->x = x;
	item->y = *y;
	item->l = w;

	if (rw && item->l > *rw) {
		*rw = item->l;
		if (*rw > w) *rw = w;
	}

	/* Height bussiness follows: */

	/* We ignore this one happily now. Rather be Dynamic ;p. */
	/* (*y) += item->item->gid; */

	/* This is only weird heuristic, it could scale well I hope. */
	optimal_h = max_y * 2 / 3 - 2 * DIALOG_TB - 8;
	min = get_opt_int("ui.dialogs.listbox_min_height");

	if (max_y - 8 < min) {
		/* Big trouble: can't satisfy even the minimum :-(. */
		item->h = max_y - 8;
	} else if (optimal_h < min) {
		item->h = min;
	} else {
		item->h = optimal_h;
	}

	/* debug("::%d(%d)::%d::%d::", max_y, term?1:2, item->h, *y); */

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
 * item _before_ the output item). If N is zero, we traverse thru the list
 * (down) until we reach the end or fn() returns 0. */
/* From the box structure, we should use only 'items' here. */
struct listbox_item *
traverse_listbox_items_list(struct listbox_item *item, int offset,
			    int follow_visible,
			    int (*fn)(struct listbox_item *, void *, int *),
			    void *d)
{
	struct listbox_item *visible_item = item;
	struct listbox_data *box;
	int levmove = 0;
	int stop = 0;
	int infinite = !offset;

	if (!item) return NULL;
	box = item->box->next;

	if (infinite)
		offset = 1;

	while (offset && !stop) {
		/* We need to cache these. Or what will happen if something
		 * will free us item too early? However, we rely on item
		 * being at least NULL in that case. */
		/* There must be no orphaned listbox_items. No free()d roots
		 * and no dangling children. */
#define	item_cache(item) \
	do { \
		croot = item->root; cprev = item->prev; cnext = item->next; \
	} while (0)
		struct listbox_item *croot, *cprev, *cnext;

		item_cache(item);

		if (fn && (!follow_visible || item->visible)) {
			if (fn(item, d, &offset)) {
				/* We was free()d! Let's try to carry on w/ the
				 * cached coordinates. */
				item = NULL;
			}
			if (!offset) {
				infinite = 0; /* safety (matches) */
				continue;
			}
		}

		if (offset > 0) {
			/* Otherwise we climb back up when last item in root
			 * is a folder. */
			struct listbox_item *cragsman = NULL;

			/* Direction DOWN. */

			if (!infinite) offset--;

			if (item && !list_empty(item->child) && item->expanded
			    && (!follow_visible || item->visible)) {
				/* Descend to children. */
				item = item->child.next;
				item_cache(item);
				goto done_down;
			}

			while (croot
			       && (void *) cnext == &croot->child) {
				/* Last item in a non-root list, climb to your
				 * root. */
				if (!cragsman) cragsman = item;
				item = croot;
				item_cache(item);
			}

			if (!croot && (void *) cnext == box->items) {
				/* Last item in the root list, quit.. */
				stop = 1;
				if (cragsman) {
					/* ..and fall back where we were. */
					item = cragsman;
					item_cache(item);
				}
			}

			/* We're not at the end of anything, go on. */
			if (!stop) {
				item = cnext;
				item_cache(item);
			}

done_down:
			if (!item || (follow_visible && !item->visible)) {
				offset++;
			} else {
				visible_item = item;
			}

		} else {
			/* Direction UP. */

			if (!infinite) offset++;

			if (croot
			    && (void *) cprev == &croot->child) {
				/* First item in a non-root list, climb to your
				 * root. */
				item = croot;
				item_cache(item);
				levmove = 1;
			}

			if (!croot && (void *) cprev == box->items) {
				/* First item in the root list, quit. */
				stop = 1;
				levmove = 1;
			}

			/* We're not at the start of anything, go on. */
			if (!levmove && !stop) {
				item = cprev;
				item_cache(item);

				while (item && !list_empty(item->child)
					&& item->expanded
					&& (!follow_visible || item->visible)) {
					/* Descend to children. */
					item = item->child.prev;
					item_cache(item);
				}
			} else {
				levmove = 0;
			}

			if (!item || (follow_visible && !item->visible)) {
				offset--;
			} else {
				visible_item = item;
			}
		}
#undef item_cache
	}

	return visible_item;
}

struct box_context {
	struct terminal *term;
	struct widget_data *listbox_item_data;
	struct listbox_data *box;
	struct dialog_data *dlg;
	int dist;
	int offset;
};

/* Takes care about listbox top moving. */
static int
box_sel_move_do(struct listbox_item *item, void *data_, int *offset)
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

	return 0;
}

/* Moves the selected item by [dist] items. If [dist] is out of the current
 * range, the selected item is moved to the extreme (ie, the top or bottom) */
void
box_sel_move(struct widget_data *listbox_item_data, int dist)
{
	struct listbox_data *box;

	box = (struct listbox_data *) listbox_item_data->item->data;
	if (!list_empty(*box->items)) {
		if (!box->top) box->top = box->items->next;
		if (!box->sel) box->sel = box->top;
	}

	/* We want to have these visible if possible. */
	if (box->top && !box->top->visible) {
		box->top = traverse_listbox_items_list(box->top,
				1, 1, NULL, NULL);
		box->sel = box->top;
	}

	if (traverse_listbox_items_list(box->sel, dist, 1, NULL, NULL)
	    != box->sel) {
		struct box_context *data = fmem_alloc(sizeof(struct box_context));

		if (data) {
			data->box = box;
			data->listbox_item_data = listbox_item_data;
			data->dist = dist;
			data->term = NULL;
			data->dlg = NULL;
			data->offset = 0;

			/* XXX: This is ugly, yes; but we don't want to call the
			 * callback if we won't move on at all. */
			box->sel = traverse_listbox_items_list(box->sel, dist, 1,
						       	       box_sel_move_do,
							       data);
			fmem_free(data);
		}
	}
}


/* Takes care about rendering of each listbox item. */
static int
display_listbox_item(struct listbox_item *item, void *data_, int *offset)
{
	struct box_context *data = data_;
	unsigned char *text = item->text;
	int len; /* Length of the current text field. */
	int color;
	int depth = item->depth + 1;
	int d;

	if (item->translated)
		text = _(text, data->term);

	len = strlen(text);
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

		for (i = depth - d; i; i--) {
			child = root;
			if (root) root = root->root;
		}

		/* XXX */
		print_text(data->term, data->listbox_item_data->x + d * 5,
			   data->listbox_item_data->y + data->offset,
			   5, "     ", color);

		if (root ? root->child.prev == child
			 : data->box->items->prev == child)
			continue; /* We were the last branch. */

		set_char(data->term, data->listbox_item_data->x + d * 5 + 1,
			 data->listbox_item_data->y + data->offset,
			 color + FRAMES_VLINE);
	}

	if (depth) {
		enum frame_char str[5] =
			{ 32, FRAMES_RTEE, FRAMES_HLINE, FRAMES_HLINE, 32 };
		int i;

		if (item->type == BI_LEAF) {
			if (item->root) {
				if (item == item->root->child.prev) {
					str[1] = FRAMES_DLCORNER;
				}
			} else {
				if (((struct listbox_data *) item->box->next)->items->next == item) {
					str[1] = FRAMES_ULCORNER;
				} else if (((struct listbox_data *) item->box->next)->items->prev == item) {
					str[1] = FRAMES_DLCORNER;
				}
			}
		} else {
			if (item->expanded) {
				str[0] = '['; str[1] = '-'; str[2] = ']';
			} else {
				str[0] = '['; str[1] = '+'; str[2] = ']';
			}
		}

		if (item->marked) str[4] = '*';

		for (i = 0; i < 5; i++) {
			set_char(data->term,
				 data->listbox_item_data->x + (depth - 1) * 5 + i,
				 data->listbox_item_data->y + data->offset,
				 color + str[i]);
		}
	}

	print_text(data->term, data->listbox_item_data->x + depth * 5,
		   data->listbox_item_data->y + data->offset,
		   len, text, color);
	if (item == data->box->sel) {
		/* For blind users: */
		set_cursor(data->term,
			data->listbox_item_data->x, data->listbox_item_data->y + data->offset,
			data->listbox_item_data->x, data->listbox_item_data->y + data->offset);
		set_window_ptr(data->dlg->win,
			data->listbox_item_data->x, data->listbox_item_data->y + data->offset);
	}

	data->offset++;

	return 0;
}

/* Displays a dialog box */
static void
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


	/* We want to have these visible if possible. */
	if (box->top && !box->top->visible) {
		/* debug("top: %s - (%d) %p\n", box->top->text, box->top->visible, box->top); */
		box->top = traverse_listbox_items_list(box->top, 1,
				1, NULL, NULL);
		box->sel = box->top;
	}

	data = fmem_alloc(sizeof(struct box_context));
	if (data) {
		data->term = term;
		data->listbox_item_data = listbox_item_data;
		data->box = box;
		data->dlg = dlg;
		data->offset = 0;
		data->dist = 0;

		traverse_listbox_items_list(box->top, listbox_item_data->h,
					    1, display_listbox_item, data);

		fmem_free(data);
	}
}

static void
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

static int
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
			int offset = ev->y - di->y;

			box->sel_offset = offset;
			if (offset)
			box->sel = traverse_listbox_items_list(box->top,
							       offset, 1,
							       NULL, NULL);
			else box->sel = box->top;

			if (box->sel && ev->x >= di->x + box->sel->depth * 5
			    && ev->x <= di->x + box->sel->depth * 5 + 2) {
				box->sel->expanded = !box->sel->expanded;
			}

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

static int
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

			if (ev->x == KBD_INS || ev->x == '*') {
				struct listbox_data *box;

				box = (struct listbox_data *) dlg->items[dlg->n - 1].item->data;
				if (box->sel) {
					box->sel->marked = !box->sel->marked;
					box_sel_move(&dlg->items[dlg->n - 1], 1);
				}
				display_dlg_item(dlg, &dlg->items[dlg->n - 1], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_DEL) {
				struct listbox_data *box;

				box = (struct listbox_data *) dlg->items[dlg->n - 1].item->data;
				if (box->ops && box->ops->del)
					box->ops->del(dlg->win->term, box);

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
