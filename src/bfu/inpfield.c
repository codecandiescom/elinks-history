/* Input field widget implementation. */
/* $Id: inpfield.c,v 1.39 2003/07/31 15:04:16 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/memlist.h"
#include "util/memory.h"


int
check_number(struct dialog_data *dlg, struct widget_data *di)
{
	unsigned char *end;
	long l;

	errno = 0;
	l = strtol(di->cdata, (char **)&end, 10);

	if (errno || !*di->cdata || *end) {
		msg_box(dlg->win->term, NULL, 0,
			N_("Bad number"), AL_CENTER,
			N_("Number expected in field"),
			NULL, 1,
			N_("Cancel"),	NULL, B_ENTER | B_ESC);
		return 1;
	}

	if (l < di->item->gid || l > di->item->gnum) {
		msg_box(dlg->win->term, NULL, 0,
			N_("Bad number"), AL_CENTER,
			N_("Number out of range"),
			NULL, 1,
			N_("Cancel"),	NULL, B_ENTER | B_ESC);
		return 1;
	}

	return 0;
}

int
check_nonempty(struct dialog_data *dlg, struct widget_data *di)
{
	unsigned char *p;

	for (p = di->cdata; *p; p++)
		if (*p > ' ')
			return 0;

	msg_box(dlg->win->term, NULL, 0,
		N_("Bad string"), AL_CENTER,
		N_("Empty string not allowed"),
		NULL, 1,
		N_("Cancel"),	NULL, B_ENTER | B_ESC);

	return 1;
}

void
dlg_format_field(struct terminal *term, struct terminal *t2,
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
	(*y)++;
}

static int
input_field_cancel(struct dialog_data *dlg, struct widget_data *di)
{
	void (*fn)(void *) = di->item->udata;
	void *data = dlg->dlg->udata2;

	if (fn) fn(data);
	cancel_dialog(dlg, di);

	return 0;
}

static int
input_field_ok(struct dialog_data *dlg, struct widget_data *di)
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

void
input_field_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	int dialog_text_color = get_bfu_color(term, "dialog.text");

	text_width(term, dlg->dlg->udata, &min, &max);
	buttons_width(term, dlg->items + 1, 2, &min, &max);

	if (max < dlg->dlg->items->dlen) max = dlg->dlg->items->dlen;

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;

	rw = 0; /* !!! FIXME: input field */
	dlg_format_text(NULL, term, dlg->dlg->udata, 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);
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
			&y, w, NULL, dialog_text_color, AL_LEFT);
	dlg_format_field(term, term, dlg->items, dlg->x + DIALOG_LB,
			 &y, w, NULL, AL_LEFT);

	y++;
	dlg_format_buttons(term, term, dlg->items + 1, 2, dlg->x + DIALOG_LB,
			   &y, w, NULL, AL_CENTER);
}

void
input_field(struct terminal *term, struct memory_list *ml, int intl,
	    unsigned char *title,
	    unsigned char *text,
	    unsigned char *okbutton,
	    unsigned char *cancelbutton,
	    void *data, struct input_history *history, int l,
	    unsigned char *def, int min, int max,
	    int (*check)(struct dialog_data *, struct widget_data *),
	    void (*fn)(void *, unsigned char *),
	    void (*cancelfn)(void *))
{
	struct dialog *dlg;
	unsigned char *field;

	if (intl) {
		title = _(title, term);
		text = _(text, term);
		okbutton = _(okbutton, term);
		cancelbutton = _(cancelbutton, term);
	}

#define SIZEOF_DIALOG (sizeof(struct dialog) + 4 * sizeof(struct widget))

	dlg = mem_calloc(1, SIZEOF_DIALOG + l);
	if (!dlg) return;

	field = (unsigned char *) dlg + SIZEOF_DIALOG;
	*field = 0;

#undef SIZEOF_DIALOG

	if (def) {
		int defsize = strlen(def) + 1;

		memcpy(field, def, (defsize > l) ? l - 1 : defsize);
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


static inline void
display_field_do(struct widget_data *di, struct dialog_data *dlg, int sel,
		 int hide)
{
	struct terminal *term = dlg->win->term;

	if (di->vpos + di->l <= di->cpos)
		di->vpos = di->cpos - di->l + 1;
	if (di->vpos > di->cpos)
		di->vpos = di->cpos;
	if (di->vpos < 0)
		di->vpos = 0;

	fill_area(term, di->x, di->y, di->l, 1, ' ',
			get_bfu_color(term, "dialog.field"));

	{
		int len = strlen(di->cdata + di->vpos);
		int l = (len <= di->l) ? len : di->l;

		if (!hide) {
			print_text(term, di->x, di->y, l,
				   di->cdata + di->vpos,
				   get_bfu_color(term, "dialog.field-text"));
		} else {
			fill_area(term, di->x, di->y, l, 1, '*',
				  get_bfu_color(term, "dialog.field-text"));
		}
	}

	if (sel) {
		int x = di->x + di->cpos - di->vpos;

		set_cursor(term, x, di->y, 0);
		set_window_ptr(dlg->win, di->x, di->y);
	}
}

static inline void
display_field(struct widget_data *di, struct dialog_data *dlg, int sel)
{
	display_field_do(di, dlg, sel, 0);
}

static inline void
display_field_pass(struct widget_data *di, struct dialog_data *dlg, int sel)
{
	display_field_do(di, dlg, sel, 1);
}

static void
init_field(struct widget_data *widget, struct dialog_data *dialog,
	   struct event *ev)
{
	if (widget->item->history) {
		struct input_history_item *item;

		foreach (item, widget->item->history->items) {
			int dsize = strlen(item->d) + 1;
			struct input_history_item *hi;

			hi = mem_alloc(sizeof(struct input_history_item)
					+ dsize);
			if (!hi) continue;

			memcpy(hi->d, item->d, dsize);
			add_to_list(widget->history, hi);
		}
	}

	widget->cpos = strlen(widget->cdata);
}

static int
mouse_field(struct widget_data *di, struct dialog_data *dlg, struct event *ev)
{
	if (ev->y != di->y || ev->x < di->x
	    || ev->x >= di->x + di->l)
		return EVENT_NOT_PROCESSED;

	switch (ev->b & BM_BUTT) {
		case B_WHEEL_UP:
			if ((ev->b & BM_ACT) == B_DOWN &&
			    (void *) di->cur_hist->prev != &di->history) {
				di->cur_hist = di->cur_hist->prev;
				dlg_set_history(di);
				goto dsp_f;
			}
			return EVENT_PROCESSED;

		case B_WHEEL_DOWN:
			if ((ev->b & BM_ACT) == B_DOWN &&
			    (void *) di->cur_hist != &di->history) {
				di->cur_hist = di->cur_hist->next;
				dlg_set_history(di);
				goto dsp_f;
			}
			return EVENT_PROCESSED;
	}

	di->cpos = di->vpos + ev->x - di->x;
	{
		int len = strlen(di->cdata);

		if (di->cpos > len)
			di->cpos = len;
	}
	display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
	dlg->selected = di - dlg->items;

dsp_f:
	display_dlg_item(dlg, di, 1);
	return EVENT_PROCESSED;
}

/* XXX: The world's best candidate for massive goto cleanup! --pasky */
static int
kbd_field(struct widget_data *di, struct dialog_data *dlg, struct event *ev)
{
	struct window *win = dlg->win;
	struct terminal *term = win->term;

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

				if (di->cpos >= cdata_len) goto dsp_f;

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

		case ACT_PASTE_CLIPBOARD:
			{
				/* Paste from clipboard */
				unsigned char *clipboard = get_clipboard_text();

				if (!clipboard) goto dsp_f;

				safe_strncpy(di->cdata, clipboard, di->item->dlen);
				di->cpos = strlen(di->cdata);
				mem_free(clipboard);
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

				if (cdata_len >= di->item->dlen - 1)
					goto dsp_f;

				memmove(di->cdata + di->cpos + 1,
					di->cdata + di->cpos,
					cdata_len - di->cpos + 1);
				di->cdata[di->cpos++] = ev->x;
				goto dsp_f;
			}
	}
	return EVENT_NOT_PROCESSED;

dsp_f:
	display_dlg_item(dlg, di, 1);
	redraw_from_window(dlg->win);
	return EVENT_PROCESSED;
}

struct widget_ops field_ops = {
	display_field,
	init_field,
	mouse_field,
	kbd_field,
};

struct widget_ops field_pass_ops = {
	display_field_pass,
	init_field,
	mouse_field,
	kbd_field,
	NULL,
};
