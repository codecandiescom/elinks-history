/* Dialog box implementation. */
/* $Id: dialog.c,v 1.13 2002/09/10 11:13:32 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/listbox.h"
#include "bfu/widget.h"
#include "config/kbdbind.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"

#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"


/* Prototypes */
void dialog_func(struct window *, struct event *, int);


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

void
redraw_dialog(struct dialog_data *dlg)
{
	int i;
	int x = dlg->x + DIALOG_LEFT_BORDER;
	int y = dlg->y + DIALOG_TOP_BORDER;
	struct terminal *term = dlg->win->term;
	int dialog_title_color = get_bfu_color(term, "dialog.title");

	draw_frame(term, x, y,
		   dlg->xw - 2 * DIALOG_LEFT_BORDER,
		   dlg->yw - 2 * DIALOG_TOP_BORDER,
		   get_bfu_color(term, "dialog.frame"), DIALOG_FRAME);

	i = strlen(_(dlg->dlg->title, term));
	x = (dlg->xw - i) / 2 + dlg->x;
	print_text(term, x - 1, y, 1, " ", dialog_title_color);
	print_text(term, x, y, i, _(dlg->dlg->title, term), dialog_title_color);
	print_text(term, x + i, y, 1, " ", dialog_title_color);

	for (i = 0; i < dlg->n; i++)
		display_dlg_item(dlg, &dlg->items[i], i == dlg->selected);

	redraw_from_window(dlg->win);
}

/* TODO: This is too long and ugly. Rewrite and split. */
void
dialog_func(struct window *win, struct event *ev, int fwd)
{
	int i;
	struct terminal *term = win->term;
	struct dialog_data *dlg = win->data;

	dlg->win = win;

	/* Use nonstandard event handlers */
	if (dlg->dlg->handle_event &&
	    (dlg->dlg->handle_event(dlg, ev) == EVENT_PROCESSED)) {
		return;
	}

	switch (ev->ev) {
		case EV_INIT:
			for (i = 0; i < dlg->n; i++) {
				struct widget_data *widget = &dlg->items[i];

				memset(widget, 0, sizeof(struct widget_data));
				widget->item = &dlg->dlg->items[i];

				widget->cdata = mem_alloc(widget->item->dlen);
				if (widget->cdata) {
					memcpy(widget->cdata,
					       widget->item->data,
					       widget->item->dlen);
				} else {
					continue;
				}

				/* XXX: REMOVE THIS! --pasky */
				{
					struct widget_ops *w_o[] = {
						NULL,
						&checkbox_ops,
						&field_ops,
						&field_pass_ops,
						&button_ops,
						&listbox_ops,
					};

					widget->item->ops =
						w_o[widget->item->type];
				}

				init_list(widget->history);
				widget->cur_hist = (struct input_history_item *)
						   &widget->history;

				if (widget->item->ops->init)
					widget->item->ops->init(widget, dlg,
								ev);
			}
			dlg->selected = 0;

		case EV_RESIZE:
		case EV_REDRAW:
			dlg->dlg->fn(dlg);
			redraw_dialog(dlg);
			break;

		case EV_MOUSE:
			for (i = 0; i < dlg->n; i++)
				if (dlg->items[i].item->ops->mouse)
					if (dlg->items[i].item->ops->mouse(&dlg->items[i], dlg, ev))
						break;
			break;

		case EV_KBD:
			{
			struct widget_data *di;

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
						{
						int cdata_len = strlen(di->cdata);

						if (di->cpos < cdata_len)
							memmove(di->cdata + di->cpos,
								di->cdata + di->cpos + 1,
								cdata_len - di->cpos + 1);
						goto dsp_f;
						}

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
							int cdata_len = strlen(di->cdata);

							if (cdata_len < di->item->dlen - 1) {
								memmove(di->cdata + di->cpos + 1,
									di->cdata + di->cpos,
									cdata_len - di->cpos + 1);
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

			if ((ev->x == KBD_ENTER && di->item->type == D_BUTTON)
			    || ev->x == KBD_ENTER || ev->x == ' ') {
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
			    || (ev->x == KBD_ENTER && (ev->y == KBD_CTRL || ev->y == KBD_ALT))) {
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
			}

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

int
check_dialog(struct dialog_data *dlg)
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

int
cancel_dialog(struct dialog_data *dlg, struct widget_data *di)
{
	delete_window(dlg->win);
	return 0;
}

int
ok_dialog(struct dialog_data *dlg, struct widget_data *di)
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
int
clear_dialog(struct dialog_data *dlg, struct widget_data *di)
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

void
center_dlg(struct dialog_data *dlg)
{
	dlg->x = (dlg->win->term->x - dlg->xw) / 2;
	dlg->y = (dlg->win->term->y - dlg->yw) / 2;
}

void
draw_dlg(struct dialog_data *dlg)
{
	fill_area(dlg->win->term, dlg->x, dlg->y, dlg->xw, dlg->yw,
		  get_bfu_color(dlg->win->term, "=dialog"));
 }
