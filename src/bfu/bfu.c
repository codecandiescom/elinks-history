/* This routines are the bones of user interface. */
/* $Id: bfu.c,v 1.24 2002/06/22 21:20:51 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "bfu/align.h"
#include "bfu/bfu.h"
#include "bfu/menu.h"
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
	struct dialog_item *d;
	int n = 0;

	for (d = dlg->items; d->type != D_END; d++) n++;

	dd = mem_alloc(sizeof(struct dialog_data) +
		       sizeof(struct dialog_item_data) * n);
	if (!dd) return NULL;

	dd->dlg = dlg;
	dd->n = n;
	dd->ml = ml;
	add_window(term, dialog_func, dd);

	return dd;
}

/* display_dlg_item() */
void display_dlg_item(struct dialog_data *dlg, struct dialog_item_data *di,
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
void dlg_select_item(struct dialog_data *dlg, struct dialog_item_data *di)
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
void dlg_set_history(struct dialog_item_data *di)
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
int dlg_mouse(struct dialog_data *dlg, struct dialog_item_data *di,
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

void
tab_compl_n(struct terminal *term, unsigned char *item, int len,
	    struct window *win)
{
	struct event ev = {EV_REDRAW, 0, 0, 0};
	struct dialog_data *dd = (struct dialog_data *) win->data;
	struct dialog_item_data *di = &(dd)->items[dd->selected];

	if (len >= di->item->dlen)
		len = di->item->dlen - 1;
	memcpy(di->cdata, item, len);
	di->cdata[len] = 0;
	di->cpos = len;
	di->vpos = 0;
	ev.x = term->x;
	ev.y = term->y;
	dialog_func(win, &ev, 0);
}

void
tab_compl(struct terminal *term, unsigned char *item, struct window *win)
{
	tab_compl_n(term, item, strlen(item), win);
}

/* Complete to last unambiguous character, and display menu for all possible
 * further completions. */
void
do_tab_compl(struct terminal *term, struct list_head *history,
	     struct window *win)
{
	struct dialog_data *dd = (struct dialog_data *) win->data;
	unsigned char *cdata = dd->items[dd->selected].cdata;
	int l = strlen(cdata);
	int n = 0;
	struct input_history_item *hi;
	struct menu_item *items = DUMMY, *i;

	foreach(hi, *history) {
		if (strncmp(cdata, hi->d, l)) continue;

		if (!(n & (ALLOC_GR - 1))) {
			i = mem_realloc(items, (n + ALLOC_GR + 1)
					       * sizeof(struct menu_item));
			if (!i) {
				mem_free(items);
				return;
			}
			items = i;
		}

		items[n].text = hi->d;
		items[n].rtext = "";
		items[n].hotkey = "";
		items[n].func = (void(*)(struct terminal *, void *, void *))tab_compl;
		items[n].rtext = "";
		items[n].data = hi->d;
		items[n].in_m = 0;
		items[n].free_i = 1;
		n++;
	}

	if (n == 1) {
		tab_compl(term, items->data, win);
		mem_free(items);
		return;
	}

	if (n) {
		memset(&items[n], 0, sizeof(struct menu_item));
		do_menu_selected(term, items, win, n - 1);
	}
}

/* Complete to the last unambiguous character. Eg., I've been to google.com,
 * google.com/search?q=foo, and google.com/search?q=bar.  This function then
 * completes `go' to `google.com' and `google.com/' to `google.com/search?q='.
 */
void
do_tab_compl_unambiguous(struct terminal *term, struct list_head *history,
			 struct window *win)
{
	struct dialog_data *dd = (struct dialog_data *) win->data;
	unsigned char *cdata = dd->items[dd->selected].cdata;
	int cdata_len = strlen(cdata);
	int match_len = cdata_len;
	/* Maximum number of characters in a match. Characters after this
	 * position are varying in other matches. Zero means that no max has
	 * been set yet. */
	int max = 0;
	unsigned char *match = NULL;
	struct input_history_item *cur;

	foreach(cur, *history) {
		unsigned char *c = cur->d - 1;
		unsigned char *m = (match ? match : cdata) - 1;
		int len = 0;

		while (*++m && *++c && *m == *c && (++len, !max || len < max));
		if (len < cdata_len)
			continue;
		if (len < match_len || (*c && m != cdata + len))
			max = len;
		match = cur->d;
		match_len = (m == cdata + len && !*m) ? strlen(cur->d) : len;
	}

	if (!match)
		return;

	tab_compl_n(term, match, match_len, win);
}

/* TODO: This is too long and ugly. Rewrite and split. */
void dialog_func(struct window *win, struct event *ev, int fwd)
{
	int i;
	struct terminal *term = win->term;
	struct dialog_data *dlg = win->data;
	struct dialog_item_data *di;

	dlg->win = win;

	/* Use nonstandard event handlers */
	if (dlg->dlg->handle_event &&
	    (dlg->dlg->handle_event(dlg, ev) == EVENT_PROCESSED)) {
		return;
	}

	switch (ev->ev) {
		case EV_INIT:
			for (i = 0; i < dlg->n; i++) {
				struct dialog_item_data *di = &dlg->items[i];

				memset(di, 0, sizeof(struct dialog_item_data));
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

			if ((ev->x == KBD_ENTER && di->item->type == D_BUTTON) || ev->x == ' ') {
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

			if (ev->x == KBD_ENTER) {
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
				struct dialog_item_data *di = &dlg->items[i];

				if (di->cdata) mem_free(di->cdata);
				free_list(di->history);
			}

			freeml(dlg->ml);
	}
}

/* check_number() */
int check_number(struct dialog_data *dlg, struct dialog_item_data *di)
{
	unsigned char *end;
	long l = strtol(di->cdata, (char **)&end, 10);

	if (!*di->cdata || *end) {
		msg_box(dlg->win->term, NULL,
			TEXT(T_BAD_NUMBER), AL_CENTER,
			TEXT(T_NUMBER_EXPECTED),
			NULL, 1,
			TEXT(T_CANCEL),	NULL, B_ENTER | B_ESC);
		return 1;
	}

	if (l < di->item->gid || l > di->item->gnum) {
		msg_box(dlg->win->term, NULL,
			TEXT(T_BAD_NUMBER), AL_CENTER,
			TEXT(T_NUMBER_OUT_OF_RANGE),
			NULL, 1,
			TEXT(T_CANCEL),	NULL, B_ENTER | B_ESC);
		return 1;
	}

	return 0;
}

/* check_nonempty() */
int check_nonempty(struct dialog_data *dlg, struct dialog_item_data *di)
{
	unsigned char *p;

	for (p = di->cdata; *p; p++)
		if (*p > ' ')
			return 0;

	msg_box(dlg->win->term, NULL,
		TEXT(T_BAD_STRING), AL_CENTER,
		TEXT(T_EMPTY_STRING_NOT_ALLOWED),
		NULL, 1,
		TEXT(T_CANCEL),	NULL, B_ENTER | B_ESC);

	return 1;
}

/* cancel_dialog() */
int cancel_dialog(struct dialog_data *dlg, struct dialog_item_data *di)
{
	delete_window(dlg->win);
	return 0;
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

/* ok_dialog() */
int ok_dialog(struct dialog_data *dlg, struct dialog_item_data *di)
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
int clear_dialog(struct dialog_data *dlg, struct dialog_item_data *di)
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

/* max_text_width() */
void max_text_width(struct terminal *term, unsigned char *text, int *width)
{
	text = _(text, term);
	do {
		int c = 0;

		while (*text && *text != '\n') {
			text++;
			c++;
		}
		if (c > *width) *width = c;
	} while (*(text++));
}

/* min_text_width() */
void min_text_width(struct terminal *term, unsigned char *text, int *width)
{
	text = _(text, term);
	do {
		int c = 0;

		while (*text && *text != '\n' && *text != ' ') {
			text++;
			c++;
		}
		if (c > *width) *width = c;
	} while (*(text++));
}

/* Format text according to dialog dimensions and alignment. */
/* TODO: Longer names for local variables. */
void dlg_format_text(struct terminal *term, struct terminal *t2,
		     unsigned char *text, int x, int *y, int w,	int *rw,
		     int co, enum format_align align)
{
	text = _(text, t2);
	do {
		unsigned char *tx;
		unsigned char *tt = text;
		int s;
		int xx = x;
		int ww;

		do {
			while (*text && *text != '\n' && *text != ' ') {
#if 0
				if (term)
					set_char(term, xx, *y, co | *text);
#endif
				text++;
			       	xx++;
			}

			tx = ++text;
			ww = xx - x;
			xx++;
			if (*(text - 1) != ' ') break;

			while (*tx && *tx != '\n' && *tx != ' ')
				tx++;
		} while (tx - text < w - ww);

		s = (align & AL_MASK) == AL_CENTER ? (w - ww) / 2 : 0;

		if (s < 0) s = 0;

		while (tt < text - 1) {
			if (s >= w) {
				s = 0;
			   	(*y)++;
				if (rw) *rw = w;
				rw = NULL;
			}
			if (term) set_char(term, x + s, *y, co | *tt);
			s++;
			tt++;
		}
		if (rw && ww > *rw) *rw = ww;
		(*y)++;
	} while (*(text - 1));
}

/* max_buttons_width() */
void max_buttons_width(struct terminal *term, struct dialog_item_data *butt,
		       int n, int *width)
{
	int w = -2;
	int i;

	for (i = 0; i < n; i++)
		w += strlen(_((butt++)->item->text, term)) + 6;
	if (w > *width) *width = w;
}

/* min_buttons_width() */
void min_buttons_width(struct terminal *term, struct dialog_item_data *butt,
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
			struct dialog_item_data *butt, int n,
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

/* dlg_format_checkbox() */
void dlg_format_checkbox(struct terminal *term, struct terminal *t2,
			 struct dialog_item_data *chkb,
			 int x, int *y, int w, int *rw,
			 unsigned char *text)
{
	if (term) {
		chkb->x = x;
		chkb->y = *y;
	}

	if (rw) *rw -= 4;
	dlg_format_text(term, t2, text, x + 4, y, w - 4, rw,
			COLOR_DIALOG_CHECKBOX_TEXT, AL_LEFT);
	if (rw) *rw += 4;
}

/* dlg_format_checkboxes() */
void dlg_format_checkboxes(struct terminal *term, struct terminal *t2,
			   struct dialog_item_data *chkb, int n,
			   int x, int *y, int w, int *rw,
			   unsigned char **texts)
{
	while (n) {
		dlg_format_checkbox(term, t2, chkb, x, y, w, rw,
				    _(texts[0], t2));
		texts++;
		chkb++;
		n--;
	}
}

/* checkboxes_width() */
void checkboxes_width(struct terminal *term, unsigned char **texts, int *w,
		      void (*fn)(struct terminal *, unsigned char *, int *))
{
	while (texts[0]) {
		*w -= 4;
		fn(term, _(texts[0], term), w);
		*w += 4;
		texts++;
	}
}

/* dlg_format_field() */
void dlg_format_field(struct terminal *term, struct terminal *t2,
		      struct dialog_item_data *item,
		      int x, int *y, int w, int *rw, enum format_align align)
{
	item->x = x;
	item->y = *y;
	item->l = w;

	if (rw && item->l > *rw) {
		*rw = item->l;
		if (*rw > w) *rw = w;
	}
	(*y)++;
}

/* Layout for generic boxes */
void dlg_format_box(struct terminal *term, struct terminal *t2,
		    struct dialog_item_data *item,
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

/* max_group_width() */
void max_group_width(struct terminal *term, unsigned char **texts,
		     struct dialog_item_data *item, int n, int *w)
{
	int ww = 0;

	while (n--) {
		int wx;

		if (item->item->type == D_CHECKBOX) {
			wx = 4;
		} else if (item->item->type == D_BUTTON) {
			wx = strlen(_(item->item->text, term)) + 5;
		} else {
			wx = item->item->dlen + 1;
		}

		wx += strlen(_(texts[0], term));
		if (n) wx++;
		ww += wx;
		texts++;
		item++;
	}

	if (ww > *w) *w = ww;
}

/* min_group_width() */
void min_group_width(struct terminal *term, unsigned char **texts,
		     struct dialog_item_data *item, int n, int *w)
{
	while (n--) {
		int wx;

		if (item->item->type == D_CHECKBOX) {
			wx = 4;
		} else if (item->item->type == D_BUTTON) {
			wx = strlen(_(item->item->text, term)) + 5;
		} else {
			wx = item->item->dlen + 1;
		}

		wx += strlen(_(texts[0], term));
		if (wx > *w) *w = wx;
		texts++;
		item++;
	}
}

/* dlg_format_group() */
void dlg_format_group(struct terminal *term, struct terminal *t2,
		      unsigned char **texts, struct dialog_item_data *item,
		      int n, int x, int *y, int w, int *rw)
{
	int nx = 0;

	while (n--) {
		int sl;
		int wx;

		if (item->item->type == D_CHECKBOX) {
			wx = 4;
		} else if (item->item->type == D_BUTTON) {
			wx = strlen(_(item->item->text, t2)) + 5;
		} else {
			wx = item->item->dlen + 1;
		}

		if (_(texts[0], t2)[0]) {
			sl = strlen(_(texts[0], t2));
		} else {
			sl = -1;
		}

		wx += sl;
		if (nx && nx + wx > w) {
			nx = 0;
			(*y) += 2;
		}

		if (term) {
			print_text(term, x + nx + 4 * (item->item->type == D_CHECKBOX),
				   *y, strlen(_(texts[0], t2)),	_(texts[0], t2),
				   COLOR_DIALOG_TEXT);
			item->x = x + nx + (sl + 1) * (item->item->type != D_CHECKBOX);
			item->y = *y;
			if (item->item->type == D_FIELD ||
			    item->item->type == D_FIELD_PASS)
				item->l = item->item->dlen;
		}

		if (rw && nx + wx > *rw) {
			*rw = nx + wx;
			if (*rw > w) *rw = w;
		}
		nx += wx + 1;
		texts++;
		item++;
	}
	(*y)++;
}

/* checkbox_list_fn() */
void checkbox_list_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;

	checkboxes_width(term, dlg->dlg->udata, &max, max_text_width);
	checkboxes_width(term, dlg->dlg->udata, &min, min_text_width);
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 5) w = 5;

	rw = 0;
	dlg_format_checkboxes(NULL, term, dlg->items, dlg->n - 2, 0, &y, w,
			      &rw, dlg->dlg->udata);

	y++;
	dlg_format_buttons(NULL, term, dlg->items + dlg->n - 2, 2, 0, &y, w,
			   &rw, AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB + 1;
	dlg_format_checkboxes(term, term, dlg->items, dlg->n - 2,
			      dlg->x + DIALOG_LB, &y, w, NULL,
			      dlg->dlg->udata);

	y++;
	dlg_format_buttons(term, term, dlg->items + dlg->n - 2, 2,
			   dlg->x + DIALOG_LB, &y, w, &rw,
			   AL_CENTER);
}

/* group_fn() */
void group_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;

	max_group_width(term, dlg->dlg->udata, dlg->items, dlg->n - 2, &max);
	min_group_width(term, dlg->dlg->udata, dlg->items, dlg->n - 2, &min);
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;

	rw = 0;
	dlg_format_group(NULL, term, dlg->dlg->udata, dlg->items, dlg->n - 2,
			 0, &y, w, &rw);

	y++;
	dlg_format_buttons(NULL, term, dlg->items + dlg->n - 2, 2, 0, &y, w,
			   &rw, AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB + 1;
	dlg_format_group(term, term, dlg->dlg->udata, dlg->items, dlg->n - 2,
			 dlg->x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_buttons(term, term, dlg->items + dlg->n - 2, 2,
			   dlg->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}

/* msg_box_fn() */
void msg_box_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	unsigned char **ptr;
	unsigned char *text = init_str();
	int textl = 0;

	for (ptr = dlg->dlg->udata; *ptr; ptr++)
		add_to_str(&text, &textl, _(*ptr, term));

	max_text_width(term, text, &max);
	min_text_width(term, text, &min);
	max_buttons_width(term, dlg->items, dlg->n, &max);
	min_buttons_width(term, dlg->items, dlg->n, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;

	rw = 0;
	dlg_format_text(NULL, term, text, 0, &y, w, &rw, COLOR_DIALOG_TEXT,
			dlg->dlg->align);

	y++;
	dlg_format_buttons(NULL, term, dlg->items, dlg->n, 0, &y, w, &rw,
			   AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB + 1;
	dlg_format_text(term, term, text, dlg->x + DIALOG_LB, &y, w, NULL,
			COLOR_DIALOG_TEXT, dlg->dlg->align);

	y++;
	dlg_format_buttons(term, term, dlg->items, dlg->n, dlg->x + DIALOG_LB,
			   &y, w, NULL, AL_CENTER);

	mem_free(text);
}

/* msg_box_button() */
int msg_box_button(struct dialog_data *dlg, struct dialog_item_data *di)
{
	void (*fn)(void *) = (void (*)(void *)) di->item->udata;
	void *data = dlg->dlg->udata2;

	if (fn) fn(data);
	cancel_dialog(dlg, di);

	return 0;
}

/* The '...' means:
 *
 * ( text1, [text2, ..., textN, NULL,]
 *   udata, M,
 *   label1, handler1, flags1,
 *   ...,
 *   labelM, handlerM, flagsM )
 *
 * If !(align & AL_EXTD_TEXT), only one text is accepted, if you'll give it
 * AL_EXTD_TEXT, more texts are accepted, terminated by NULL.
 *
 * When labelX == NULL, the entire record is skipped.
 *
 * Handler takes one (void *), and udata is passed as it.
 *
 * You should always align it in a similiar way. */
void msg_box(struct terminal *term, struct memory_list *ml,
	     unsigned char *title, enum format_align align,
	     ...)
{
	unsigned char **info = DUMMY;
	int info_n = 0;
	int button;
	int buttons;
	struct dialog *dlg;
	void *udata;
	va_list ap;

	va_start(ap, align);

	if (align & AL_EXTD_TEXT) {
		unsigned char *text = "";

		while (text) {
			text = va_arg(ap, unsigned char *);

			info_n++;
			info = mem_realloc(info, info_n
						 * sizeof(unsigned char *));
			if (!info) {
				va_end(ap);
				return;
			}

			info[info_n - 1] = text;
		}

	} else {
		/* I had to decide between evil gotos and code duplication. */
		unsigned char *text = va_arg(ap, unsigned char *);
		unsigned char **info_;

		info_n = 2;
		info_ = mem_realloc(info, info_n
					  * sizeof(unsigned char *));
		if (!info_) {
			free(info);
			va_end(ap);
			return;
	   	}

		info = info_;
		info[0] = text;
		info[1] = NULL;
	}

	udata = va_arg(ap, void *);
	buttons = va_arg(ap, int);

#define SIZEOF_DIALOG \
	(sizeof(struct dialog) + (buttons + 1) * sizeof(struct dialog_item))

	dlg = mem_alloc(SIZEOF_DIALOG);
	if (!dlg) {
		mem_free(info);
		va_end(ap);
		return;
	}
	memset(dlg, 0, SIZEOF_DIALOG);

#undef SIZEOF_DIALOG

	dlg->title = title;
	dlg->fn = msg_box_fn;
	dlg->udata = info;
	dlg->udata2 = udata;
	dlg->align = align;

	for (button = 0; button < buttons; button++) {
		unsigned char *label;
		void (*fn)(void *);
		int flags;

		label = va_arg(ap, unsigned char *);
		fn = va_arg(ap, void *);
		flags = va_arg(ap, int);

		if (!label) {
			/* Skip this button. */
			button--;
			buttons--;
			continue;
		}

		dlg->items[button].type = D_BUTTON;
		dlg->items[button].gid = flags;
		dlg->items[button].fn = msg_box_button;
		dlg->items[button].dlen = 0;
		dlg->items[button].text = label;
		dlg->items[button].udata = fn;
	}

	va_end(ap);

	dlg->items[button].type = D_END;
	add_to_ml(&ml, dlg, info, NULL);
	do_dialog(term, dlg, ml);
}


/* FIXME: Move these history related functions elsewhere. --Zas */
/* Search duplicate entries in history list and remove older ones. */
static void remove_duplicate_from_history(struct input_history *historylist,
					  unsigned char *url)
{
	struct input_history_item *historyitem;

	if (!historylist || !url || !*url) return;

	foreach(historyitem, historylist->items) {
		if (!strcmp(historyitem->d, url)) {
			struct input_history_item *tmphistoryitem = historyitem;

			/* found a duplicate -> remove it from history list */
			historyitem = historyitem->prev;
			del_from_list(tmphistoryitem);
			mem_free(tmphistoryitem);
			historylist->n--;
		}
	}
}

/* Add a new entry in inputbox history list, take care of duplicate if
 * check_duplicate and respect history size limit. */
void add_to_input_history(struct input_history *historylist, unsigned char *url,
			  int check_duplicate)
{
	struct input_history_item *newhistoryitem;
	int url_len;

	if (!historylist || !url)
		return;

	/* Strip spaces at the margins */

	while (*url == ' ') url++;
	if (!*url) return;

	url_len = strlen(url);
	while (url_len > 0 && url[url_len - 1] == ' ') url_len--;
	if (!url_len) return;

	/* Copy it all etc. */

	newhistoryitem = mem_alloc(sizeof(struct input_history_item) + url_len + 1);
	if (!newhistoryitem) return;

	memcpy(newhistoryitem->d, url, url_len);
	newhistoryitem->d[url_len] = 0;

	if (check_duplicate)
		remove_duplicate_from_history(historylist, newhistoryitem->d);

	/* add new entry to history list */
	add_to_list(historylist->items, newhistoryitem);
	historylist->n++;

	/* limit size of history to MAX_HISTORY_ITEMS
	 * removing first entries if needed */
	while (historylist->n > MAX_HISTORY_ITEMS) {
		struct input_history_item *tmphistoryitem = historylist->items.prev;

		if ((void *) tmphistoryitem == &historylist->items) {
			internal("history is empty");
			historylist->n = 0;
			return;
		}

		del_from_list(tmphistoryitem);
		mem_free(tmphistoryitem);
		historylist->n--;
	}
}

/* input_field_cancel() */
int input_field_cancel(struct dialog_data *dlg, struct dialog_item_data *di)
{
	void (*fn)(void *) = di->item->udata;
	void *data = dlg->dlg->udata2;

	if (fn) fn(data);
	cancel_dialog(dlg, di);

	return 0;
}

/* input_field_ok() */
int input_field_ok(struct dialog_data *dlg, struct dialog_item_data *di)
{
	void (*fn)(void *, unsigned char *) = di->item->udata;
	void *data = dlg->dlg->udata2;
	unsigned char *text = dlg->items->cdata;

	if (check_dialog(dlg)) return 1;

	add_to_input_history(dlg->dlg->items->history, text, 1);

	if (fn) fn(data, text);
	ok_dialog(dlg, di);
	return 0;
}

/* input_field_fn() */
void input_field_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;

	max_text_width(term, dlg->dlg->udata, &max);
	min_text_width(term, dlg->dlg->udata, &min);
	max_buttons_width(term, dlg->items + 1, 2, &max);
	min_buttons_width(term, dlg->items + 1, 2, &min);

	if (max < dlg->dlg->items->dlen) max = dlg->dlg->items->dlen;

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;

	rw = 0; /* !!! FIXME: input field */
	dlg_format_text(NULL, term, dlg->dlg->udata, 0, &y, w, &rw,
			COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(NULL, term, dlg->items, 0, &y, w, &rw,
			 AL_LEFT);

	y++;
	dlg_format_buttons(NULL, term, dlg->items + 1, 2, 0, &y, w, &rw,
			   AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term, dlg->dlg->udata, dlg->x + DIALOG_LB,
			&y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, dlg->items, dlg->x + DIALOG_LB,
			 &y, w, NULL, AL_LEFT);

	y++;
	dlg_format_buttons(term, term, dlg->items + 1, 2, dlg->x + DIALOG_LB,
			   &y, w, NULL, AL_CENTER);
}

/* input_field() */
void input_field(struct terminal *term, struct memory_list *ml,
		 unsigned char *title,
		 unsigned char *text,
		 unsigned char *okbutton,
		 unsigned char *cancelbutton,
		 void *data, struct input_history *history, int l,
		 unsigned char *def, int min, int max,
		 int (*check)(struct dialog_data *, struct dialog_item_data *),
		 void (*fn)(void *, unsigned char *),
		 void (*cancelfn)(void *))
{
	struct dialog *dlg;
	unsigned char *field;

#define SIZEOF_DIALOG (sizeof(struct dialog) + 4 * sizeof(struct dialog_item))

	dlg = mem_alloc(SIZEOF_DIALOG + l);
	if (!dlg) return;

	memset(dlg, 0, SIZEOF_DIALOG + l);
	field = (unsigned char *) dlg + SIZEOF_DIALOG;
	*field = 0;

#undef SIZEOF_DIALOG

	if (def) {
		if (strlen(def) + 1 > l)
			memcpy(field, def, l - 1);
		else
			strcpy(field, def);
	}

	dlg->title = title;
	dlg->fn = input_field_fn;
	dlg->udata = text;
	dlg->udata2 = data;

	dlg->items[0].type = D_FIELD;
	dlg->items[0].gid = min;
	dlg->items[0].gnum = max;
	dlg->items[0].fn = check;
	dlg->items[0].history = history;
	dlg->items[0].dlen = l;
	dlg->items[0].data = field;

	dlg->items[1].type = D_BUTTON;
	dlg->items[1].gid = B_ENTER;
	dlg->items[1].fn = input_field_ok;
	dlg->items[1].dlen = 0;
	dlg->items[1].text = okbutton;
	dlg->items[1].udata = fn;

	dlg->items[2].type = D_BUTTON;
	dlg->items[2].gid = B_ESC;
	dlg->items[2].fn = input_field_cancel;
	dlg->items[2].dlen = 0;
	dlg->items[2].text = cancelbutton;
	dlg->items[2].udata = cancelfn;

	dlg->items[3].type = D_END;

	add_to_ml(&ml, dlg, NULL);
	do_dialog(term, dlg, ml);
}


/* Sets the selected item to one that is visible.*/
void box_sel_set_visible(struct dialog_item_data *box_item_data, int offset)
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
void box_sel_move(struct dialog_item_data *box_item_data, int dist)
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
		       struct dialog_item_data *box_item_data)
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
