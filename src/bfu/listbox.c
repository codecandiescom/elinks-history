/* Listbox widget implementation. */
/* $Id: listbox.c,v 1.99 2003/10/26 15:59:12 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "bfu/style.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/lists.h"


/* Layout for generic boxes */
void
dlg_format_box(struct terminal *term, struct terminal *t2,
	       struct widget_data *widget_data, int x, int *y, int w, int *rw,
	       enum format_align align)
{
	int min, optimal_h, max_y = 25;

	widget_data->x = x;
	widget_data->y = *y;
	widget_data->w = w;

	if (rw) int_bounds(rw, widget_data->w, w);

	/* Height bussiness follows: */

	/* We ignore this one happily now. Rather be Dynamic ;p. */
	/* (*y) += widget_data->widget->gid; */

	/* This is only weird heuristic, it could scale well I hope. */
	if (term) max_y = term->y;
	else if (t2) max_y = t2->y;

	optimal_h = max_y * 2 / 3 - 2 * DIALOG_TB - 8;
	min = get_opt_int("ui.dialogs.listbox_min_height");

	if (max_y - 8 < min) {
		/* Big trouble: can't satisfy even the minimum :-(. */
		widget_data->h = max_y - 8;
	} else if (optimal_h < min) {
		widget_data->h = min;
	} else {
		widget_data->h = optimal_h;
	}

	/* debug("::%d(%d)::%d::%d::", max_y, term?1:2, widget_data->h, *y); */

	(*y) += widget_data->h;
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

			if (!croot && (!cnext || (void *) cnext == box->items)) {
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
	struct dialog_data *dlg_data;
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

	box = (struct listbox_data *) listbox_item_data->widget->data;
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
		struct box_context data;

		memset(&data, 0, sizeof(struct box_context));

		data.box = box;
		data.listbox_item_data = listbox_item_data;
		data.dist = dist;

		/* XXX: This is ugly, yes; but we don't want to call the
		 * callback if we won't move on at all. */
		box->sel = traverse_listbox_items_list(box->sel, dist, 1,
						       box_sel_move_do, &data);
	}
}


/* Takes care about rendering of each listbox item. */
static int
display_listbox_item(struct listbox_item *item, void *data_, int *offset)
{
	struct box_context *data = data_;
	unsigned char *text = item->text;
	unsigned char *stylename;
	int len; /* Length of the current text field. */
	struct color_pair *color;
	int depth = item->depth + 1;
	int d;
	int y;

	if (item->translated)
		text = _(text, data->term);

	len = strlen(text);
	int_upper_bound(&len, data->listbox_item_data->w - depth * 5);

	stylename = (item == data->box->sel) ? "menu.selected"
		  : ((item->marked)	     ? "menu.marked"
					     : "menu.normal");

	color = get_bfu_color(data->term, stylename);

	y = data->listbox_item_data->y + data->offset;
	for (d = 0; d < depth - 1; d++) {
		struct listbox_item *root = item;
		struct listbox_item *child = item;
		int i, x;

		for (i = depth - d; i; i--) {
			child = root;
			if (root) root = root->root;
		}

		/* XXX */
		x = data->listbox_item_data->x + d * 5;
		draw_text(data->term, x, y, "     ", 5, 0, color);

		if (root ? root->child.prev == child
			 : data->box->items->prev == child)
			continue; /* We were the last branch. */

		draw_border_char(data->term, x + 1, y, BORDER_SVLINE, color);
	}

	if (depth) {
		enum border_char str[5] =
			{ 32, BORDER_SRTEE, BORDER_SHLINE, BORDER_SHLINE, 32 };
		int i, x;

		if (item->type == BI_LEAF) {
			if (item->root) {
				if (item == item->root->child.prev) {
					str[1] = BORDER_SDLCORNER;
				}
			} else {
				struct list_head *p = ((struct listbox_data *) item->box->next)->items;

				if (p->next == item) {
					str[1] = BORDER_SULCORNER;
				} else if (p->prev == item) {
					str[1] = BORDER_SDLCORNER;
				}
			}
		} else {
			str[0] = '[';
			str[1] = (item->expanded) ? '-' : '+';
			str[2] = ']';
		}

		if (item->marked) str[4] = '*';

		x = data->listbox_item_data->x + (depth - 1) * 5;
		for (i = 0; i < 5; i++) {
			draw_border_char(data->term, x + i, y, str[i], color);
		}
	}

	draw_text(data->term, data->listbox_item_data->x + depth * 5,
		  data->listbox_item_data->y + data->offset,
		  text, len, 0, color);

	if (item == data->box->sel) {
		int x = data->listbox_item_data->x;

		/* For blind users: */
		set_cursor(data->term, x, y, 0);
		set_window_ptr(data->dlg_data->win, x, y);
	}

	data->offset++;

	return 0;
}

/* Displays a dialog box */
static void
display_listbox(struct widget_data *listbox_item_data, struct dialog_data *dlg_data,
		int sel)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = (struct listbox_data *) listbox_item_data->widget->data;
	struct box_context *data;

	if (!list_empty(*box->items)) {
		if (!box->top) box->top = box->items->next;
		if (!box->sel) box->sel = box->top;
	}

	draw_area(term, listbox_item_data->x, listbox_item_data->y,
		  listbox_item_data->w, listbox_item_data->h, ' ', 0,
		  get_bfu_color(term, "menu.normal"));


	/* We want to have these visible if possible. */
	if (box->top && !box->top->visible) {
		/* debug("top: %s - (%d) %p\n", box->top->text, box->top->visible, box->top); */
		box->top = traverse_listbox_items_list(box->top, 1,
				1, NULL, NULL);
		box->sel = box->top;
	}

	data = mem_calloc(1, sizeof(struct box_context));
	if (data) {
		data->term = term;
		data->listbox_item_data = listbox_item_data;
		data->box = box;
		data->dlg_data = dlg_data;

		traverse_listbox_items_list(box->top, listbox_item_data->h,
					    1, display_listbox_item, data);

		mem_free(data);
	}
}

static void
init_listbox(struct widget_data *widget_data, struct dialog_data *dlg_data,
	     struct term_event *ev)
{
#if 0
	/* Freed in bookmark_dialog_abort_handler() */
	widget_data->cdata = mem_alloc(sizeof(struct listbox_data));
	if (!widget_data->cdata)
		return;

	((struct listbox_data *) widget_data->cdata)->sel = NULL;
	((struct listbox_data *) widget_data->cdata)->top = NULL;

	((struct listbox_data *) widget_data->cdata)->items =
		mem_alloc(sizeof(struct list_head));
	init_list(*((struct listbox_data *) widget_data->cdata)->items);
#endif
}

static int
mouse_listbox(struct widget_data *widget_data, struct dialog_data *dlg_data,
	      struct term_event *ev)
{
#ifdef USE_MOUSE
	struct listbox_data *box = (struct listbox_data *) widget_data->widget->data;

	if (!list_empty(*box->items)) {
		if (!box->top) box->top = box->items->next;
		if (!box->sel) box->sel = box->top;
	}

	if ((ev->b & BM_ACT) == B_DOWN) {
		struct widget_data *dlg_item = &dlg_data->widgets_data[dlg_data->n - 1];

		switch (ev->b & BM_BUTT) {
			case B_WHEEL_DOWN:
				box_sel_move(dlg_item, 1);
				display_dlg_item(dlg_data, dlg_item, 1);
				return EVENT_PROCESSED;

			case B_WHEEL_UP:
				box_sel_move(dlg_item, -1);
				display_dlg_item(dlg_data, dlg_item, 1);
				return EVENT_PROCESSED;
		}
	}

	if ((ev->b & BM_ACT) == B_UP) {
		if ((ev->b & BM_BUTT) < B_WHEEL_UP &&
		    (ev->y >= widget_data->y && ev->y < widget_data->y + widget_data->h) &&
		    (ev->x >= widget_data->x && ev->x <= widget_data->x + widget_data->w)) {
			/* Clicked in the box. */
			int offset = ev->y - widget_data->y;

			box->sel_offset = offset;
			if (offset)
				box->sel = traverse_listbox_items_list(box->top,
								       offset, 1,
								       NULL, NULL);
			else box->sel = box->top;


			if (box->sel) {
				int xdepth =  widget_data->x + box->sel->depth * 5;

			       	if (ev->x >= xdepth && ev->x <= xdepth + 2)
					box->sel->expanded = !box->sel->expanded;
			}

			display_dlg_item(dlg_data, widget_data, 1);
			return EVENT_PROCESSED;
		}
	}

#endif /* USE_MOUSE */

	return EVENT_NOT_PROCESSED;
}

static int
kbd_listbox(struct widget_data *widget_data, struct dialog_data *dlg_data,
	    struct term_event *ev)
{
	int n = dlg_data->n - 1;
	struct widget_data *dlg_item = &dlg_data->widgets_data[n];

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
				display_dlg_item(dlg_data, selected_widget(dlg_data), 0);
				if (++dlg_data->selected >= n)
					dlg_data->selected = 0;
				display_dlg_item(dlg_data, selected_widget(dlg_data), 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_LEFT || (ev->x == KBD_TAB && ev->y)) {
				/* Move left */
				display_dlg_item(dlg_data, selected_widget(dlg_data), 0);
				if (--dlg_data->selected < 0)
					dlg_data->selected = n - 1;
				display_dlg_item(dlg_data, selected_widget(dlg_data), 1);

				return EVENT_PROCESSED;
			}

			/* Moving the box */
			if (ev->x == KBD_DOWN
			    || (ev->y == KBD_CTRL && upcase(ev->x) == 'N')) {
				box_sel_move(dlg_item, 1);
				display_dlg_item(dlg_data, dlg_item, 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_UP
			    || (ev->y == KBD_CTRL && upcase(ev->x) == 'P')) {
				box_sel_move(dlg_item, -1);
				display_dlg_item(dlg_data, dlg_item, 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_DOWN
			    || (ev->y == KBD_CTRL && upcase(ev->x) == 'V')
			    || (ev->y == KBD_CTRL && upcase(ev->x) == 'F')) {
				box_sel_move(dlg_item,
					     dlg_item->h / 2);
				display_dlg_item(dlg_data, dlg_item, 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_UP
			    || (ev->y == KBD_ALT && upcase(ev->x) == 'V')
			    || (ev->y == KBD_CTRL && upcase(ev->x) == 'B')) {
				box_sel_move(dlg_item,
					     -dlg_item->h / 2);
				display_dlg_item(dlg_data, dlg_item, 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_HOME
			    || (ev->y == KBD_CTRL && upcase(ev->x) == 'A')) {
				box_sel_move(dlg_item, -MAXINT);
				display_dlg_item(dlg_data, dlg_item, 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_END
			    || (ev->y == KBD_CTRL && upcase(ev->x) == 'E')) {
				box_sel_move(dlg_item, MAXINT);
				display_dlg_item(dlg_data, dlg_item, 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_INS || ev->x == '*') {
				struct listbox_data *box;

				box = (struct listbox_data *) dlg_item->widget->data;
				if (box->sel) {
					box->sel->marked = !box->sel->marked;
					box_sel_move(dlg_item, 1);
				}
				display_dlg_item(dlg_data, dlg_item, 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_DEL) {
				struct listbox_data *box;

				box = (struct listbox_data *) dlg_item->widget->data;
				if (box->ops && box->ops->del)
					box->ops->del(dlg_data->win->term, box);

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
