/* This routines are the bones of user interface. */
/* $Id: bfu.c,v 1.30 2002/07/04 15:45:38 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "bfu/bfu.h"
#include "bfu/button.h"
#include "bfu/inphist.h"
#include "config/kbdbind.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"


/* Prototypes */
void dialog_func(struct window *, struct event *, int);

/* do_dialog() */
struct dialog_data *
do_dialog(struct terminal *term, struct dialog *dlg,
	       struct memory_list *ml)
{
	struct dialog_data *dd;
	struct widget *d;
	int n = 0;

	for (d = dlg->items; d->type != D_END; d++) n++;

	dd = mem_alloc(sizeof(struct dialog_data) +
		       sizeof(struct widget_data) * n);
	if (!dd) return NULL;

	dd->dlg = dlg;
	dd->n = n;
	dd->ml = ml;
	add_window(term, dialog_func, dd);

	return dd;
}

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

/* redraw_dialog() */
void redraw_dialog(struct dialog_data *dlg)
{
	int i;
	int x = dlg->x + DIALOG_LEFT_BORDER;
	int y = dlg->y + DIALOG_TOP_BORDER;
	struct terminal *term = dlg->win->term;

	draw_frame(term, x, y,
		   dlg->xw - 2 * DIALOG_LEFT_BORDER,
		   dlg->yw - 2 * DIALOG_TOP_BORDER,
		   COLOR_DIALOG_FRAME, DIALOG_FRAME);

	i = strlen(_(dlg->dlg->title, term));
	x = (dlg->xw - i) / 2 + dlg->x;
	print_text(term, x - 1, y, 1, " ", COLOR_DIALOG_TITLE);
	print_text(term, x, y, i, _(dlg->dlg->title, term), COLOR_DIALOG_TITLE);
	print_text(term, x + i, y, 1, " ", COLOR_DIALOG_TITLE);

	for (i = 0; i < dlg->n; i++)
		display_dlg_item(dlg, &dlg->items[i], i == dlg->selected);

	redraw_from_window(dlg->win);
}

/* TODO: This is too long and ugly. Rewrite and split. */
void dialog_func(struct window *win, struct event *ev, int fwd)
{
	int i;
	struct terminal *term = win->term;
	struct dialog_data *dlg = win->data;
	struct widget_data *di;

	dlg->win = win;

	/* Use nonstandard event handlers */
	if (dlg->dlg->handle_event &&
	    (dlg->dlg->handle_event(dlg, ev) == EVENT_PROCESSED)) {
		return;
	}

	switch (ev->ev) {
		case EV_INIT:
			for (i = 0; i < dlg->n; i++) {
				struct widget_data *di = &dlg->items[i];

				memset(di, 0, sizeof(struct widget_data));
				di->item = &dlg->dlg->items[i];

				di->cdata = mem_alloc(di->item->dlen);
				if (di->cdata) {
					memcpy(di->cdata, di->item->data,
					       di->item->dlen);
				} else {
					continue;
				}

				if (di->item->type == D_CHECKBOX) {
					if (di->item->gid) {
						if (*((int *) di->cdata)
						    == di->item->gnum)
							di->checked = 1;
					} else {
						if (*((int *) di->cdata))
							di->checked = 1;
					}
				}

				if (di->item->type == D_BOX) {
					/* Freed in bookmark_dialog_abort_handler() */
					di->cdata = mem_alloc(sizeof(struct dlg_data_item_data_box));
					if (!di->cdata)
						continue;

					((struct dlg_data_item_data_box *) di->cdata)->sel = -1;
					((struct dlg_data_item_data_box *) di->cdata)->box_top = 0;
					((struct dlg_data_item_data_box *) di->cdata)->list_len = -1;

					init_list(((struct dlg_data_item_data_box*)di->cdata)->items);
				}

				init_list(di->history);
				di->cur_hist = (struct input_history_item *) &di->history;

				if (di->item->type == D_FIELD ||
				    di->item->type == D_FIELD_PASS) {
					if (di->item->history) {
						struct input_history_item *j;

						foreach(j, di->item->history->items) {
							struct input_history_item *hi;

							hi = mem_alloc(sizeof(struct input_history_item)
								       + strlen(j->d) + 1);
							if (!hi) continue;

							strcpy(hi->d, j->d);
							add_to_list(di->history, hi);
						}
					}
					di->cpos = strlen(di->cdata);
				}
			}
			dlg->selected = 0;

		case EV_RESIZE:
		case EV_REDRAW:
			dlg->dlg->fn(dlg);
			redraw_dialog(dlg);
			break;

		case EV_MOUSE:
			for (i = 0; i < dlg->n; i++)
				if (dlg_mouse(dlg, &dlg->items[i], ev))
					break;
			break;

		case EV_KBD:
			di = &dlg->items[dlg->selected];
			if (di->item->type == D_FIELD ||
			    di->item->type == D_FIELD_PASS) {
				switch (kbd_action(KM_EDIT, ev, NULL)) {
					case ACT_UP:
						if ((void *) di->cur_hist->prev != &di->history) {
							di->cur_hist = di->cur_hist->prev;
							dlg_set_history(di);
							goto dsp_f;
						}
						break;

					case ACT_DOWN:
						if ((void *) di->cur_hist != &di->history) {
							di->cur_hist = di->cur_hist->next;
							dlg_set_history(di);
							goto dsp_f;
						}
						break;

					case ACT_RIGHT:
						if (di->cpos < strlen(di->cdata)) di->cpos++;
						goto dsp_f;

					case ACT_LEFT:
						if (di->cpos > 0) di->cpos--;
						goto dsp_f;

					case ACT_HOME:
						di->cpos = 0;
						goto dsp_f;

					case ACT_END:
						di->cpos = strlen(di->cdata);
						goto dsp_f;

					case ACT_BACKSPACE:
						if (di->cpos) {
							memmove(di->cdata + di->cpos - 1,
								di->cdata + di->cpos,
								strlen(di->cdata) - di->cpos + 1);
							di->cpos--;
						}
						goto dsp_f;

					case ACT_DELETE:
						if (di->cpos < strlen(di->cdata))
							memmove(di->cdata + di->cpos,
								di->cdata + di->cpos + 1,
								strlen(di->cdata) - di->cpos + 1);
						goto dsp_f;

					case ACT_KILL_TO_BOL:
						memmove(di->cdata,
							di->cdata + di->cpos,
							strlen(di->cdata + di->cpos) + 1);
						di->cpos = 0;
						goto dsp_f;

					case ACT_KILL_TO_EOL:
						di->cdata[di->cpos] = 0;
						goto dsp_f;

					case ACT_COPY_CLIPBOARD:
 						/* Copy to clipboard */
						set_clipboard_text(di->cdata);
						break;	/* We don't need to redraw */

					case ACT_CUT_CLIPBOARD:
 						/* Cut to clipboard */
						set_clipboard_text(di->cdata);
						di->cdata[0] = 0;
						di->cpos = 0;
						goto dsp_f;

					case ACT_PASTE_CLIPBOARD: {
						/* Paste from clipboard */
						unsigned char *clipboard = get_clipboard_text();

						if (clipboard) {
							safe_strncpy(di->cdata, clipboard, di->item->dlen);
							di->cpos = strlen(di->cdata);
							mem_free(clipboard);
						}
						goto dsp_f;
					}

					case ACT_AUTO_COMPLETE:
						do_tab_compl(term, &di->history, win);
						goto dsp_f;

					case ACT_AUTO_COMPLETE_UNAMBIGUOUS:
						do_tab_compl_unambiguous(term, &di->history, win);
						goto dsp_f;

					default:
						if (ev->x >= ' ' && ev->x < 0x100 && !ev->y) {
							if (strlen(di->cdata) < di->item->dlen - 1) {
								memmove(di->cdata + di->cpos + 1,
									di->cdata + di->cpos,
									strlen(di->cdata) - di->cpos + 1);
								di->cdata[di->cpos++] = ev->x;
							}
							goto dsp_f;
						}
				}
				goto gh;

dsp_f:
				display_dlg_item(dlg, di, 1);
				redraw_from_window(dlg->win);
				break;
			}

			if ((ev->x == KBD_ENTER && di->item->type == D_BUTTON) || ev->x == KBD_ENTER) {
				dlg_select_item(dlg, di);
				break;
			}
gh:
			if (ev->x > ' ' && ev->x < 0x100) {
				for (i = 0; i < dlg->n; i++)
					if (dlg->dlg->items[i].type == D_BUTTON
					    && upcase(_(dlg->dlg->items[i].text, term)[0])
					       == upcase(ev->x)) {
sel:
						if (dlg->selected != i) {
							display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
							display_dlg_item(dlg, &dlg->items[i], 1);
							dlg->selected = i;
						}
						dlg_select_item(dlg, &dlg->items[i]);
						return;
					}
			}

			if ((ev->x == KBD_ENTER && (di->item->type == D_FIELD ||
						    di->item->type == D_FIELD_PASS))
			    || ev->x == ' ') {
				for (i = 0; i < dlg->n; i++)
					if (dlg->dlg->items[i].type == D_BUTTON
					    && dlg->dlg->items[i].gid & B_ENTER)
						goto sel;
			}

			if (ev->x == KBD_ESC) {
				for (i = 0; i < dlg->n; i++)
					if (dlg->dlg->items[i].type == D_BUTTON
					    && dlg->dlg->items[i].gid & B_ESC)
						goto sel;
			}

			if ((ev->x == KBD_TAB && !ev->y) ||
			     ev->x == KBD_DOWN ||
			     ev->x == KBD_RIGHT) {
				display_dlg_item(dlg, &dlg->items[dlg->selected], 0);

				dlg->selected++;
				if (dlg->selected >= dlg->n)
					dlg->selected = 0;

				display_dlg_item(dlg, &dlg->items[dlg->selected], 1);
				redraw_from_window(dlg->win);
				break;
			}

			if ((ev->x == KBD_TAB && ev->y) ||
			     ev->x == KBD_UP ||
			     ev->x == KBD_LEFT) {
				display_dlg_item(dlg, &dlg->items[dlg->selected], 0);

				dlg->selected--;
				if (dlg->selected < 0)
					dlg->selected = dlg->n - 1;

				display_dlg_item(dlg, &dlg->items[dlg->selected], 1);
				redraw_from_window(dlg->win);
				break;
			}
			break;

		case EV_ABORT:
			/* Moved this line up so that the dlg would have access
			   to its member vars before they get freed. */
			if (dlg->dlg->abort)
				dlg->dlg->abort(dlg);

			for (i = 0; i < dlg->n; i++) {
				struct widget_data *di = &dlg->items[i];

				if (di->cdata) mem_free(di->cdata);
				free_list(di->history);
			}

			freeml(dlg->ml);
	}
}

/* check_dialog() */
int check_dialog(struct dialog_data *dlg)
{
	int i;

	for (i = 0; i < dlg->n; i++) {
		if (dlg->dlg->items[i].type == D_CHECKBOX ||
		    dlg->dlg->items[i].type == D_FIELD ||
		    dlg->dlg->items[i].type == D_FIELD_PASS) {
			if (dlg->dlg->items[i].fn &&
			    dlg->dlg->items[i].fn(dlg, &dlg->items[i])) {
				dlg->selected = i;
				redraw_dialog(dlg);
				return 1;
			}
		}
	}
	return 0;
}

/* cancel_dialog() */
int cancel_dialog(struct dialog_data *dlg, struct widget_data *di)
{
	delete_window(dlg->win);
	return 0;
}

/* ok_dialog() */
int ok_dialog(struct dialog_data *dlg, struct widget_data *di)
{
	int i;
	void (*fn)(void *) = dlg->dlg->refresh;
	void *data = dlg->dlg->refresh_data;

	if (check_dialog(dlg)) return 1;

	for (i = 0; i < dlg->n; i++)
		memcpy(dlg->dlg->items[i].data,
		       dlg->items[i].cdata,
		       dlg->dlg->items[i].dlen);

	if (fn) fn(data);
	i = cancel_dialog(dlg, di);
	return i;
}

/* FIXME? Added to clear fields in bookmarks dialogs, may be broken if used
 * elsewhere. --Zas */
int clear_dialog(struct dialog_data *dlg, struct widget_data *di)
{
	int i;

	for (i = 0; i < dlg->n; i++) {
		if (dlg->dlg->items[i].type == D_FIELD ||
		    dlg->dlg->items[i].type == D_FIELD_PASS) {
			memset(dlg->items[i].cdata, 0, dlg->dlg->items[i].dlen);
			dlg->items[i].cpos = 0;
		}
	}

	redraw_dialog(dlg);
	return 0;
}

/* center_dlg() */
void center_dlg(struct dialog_data *dlg)
{
	dlg->x = (dlg->win->term->x - dlg->xw) / 2;
	dlg->y = (dlg->win->term->y - dlg->yw) / 2;
}

/* draw_dlg() */
void draw_dlg(struct dialog_data *dlg)
{
	fill_area(dlg->win->term, dlg->x, dlg->y, dlg->xw, dlg->yw,
		  COLOR_DIALOG);
}


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
void show_dlg_item_box(struct dialog_data *dlg,
		       struct widget_data *box_item_data)
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
