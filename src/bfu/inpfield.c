/* Input field widget implementation. */
/* $Id: inpfield.c,v 1.59 2003/10/26 12:52:32 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/memlist.h"
#include "util/memory.h"


int
check_number(struct dialog_data *dlg_data, struct widget_data *di)
{
	unsigned char *end;
	long l;

	errno = 0;
	l = strtol(di->cdata, (char **)&end, 10);

	if (errno || !*di->cdata || *end) {
		msg_box(dlg_data->win->term, NULL, 0,
			N_("Bad number"), AL_CENTER,
			N_("Number expected in field"),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		return 1;
	}

	if (l < di->widget->gid || l > di->widget->gnum) {
		msg_box(dlg_data->win->term, NULL, 0,
			N_("Bad number"), AL_CENTER,
			N_("Number out of range"),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		return 1;
	}

	return 0;
}

int
check_nonempty(struct dialog_data *dlg_data, struct widget_data *di)
{
	unsigned char *p;

	for (p = di->cdata; *p; p++)
		if (*p > ' ')
			return 0;

	msg_box(dlg_data->win->term, NULL, 0,
		N_("Bad string"), AL_CENTER,
		N_("Empty string not allowed"),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);

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

	if (rw && w > *rw) *rw = w;
	(*y)++;
}

static int
input_field_cancel(struct dialog_data *dlg_data, struct widget_data *di)
{
	void (*fn)(void *) = di->widget->udata;
	void *data = dlg_data->dlg->udata2;

	if (fn) fn(data);
	cancel_dialog(dlg_data, di);

	return 0;
}

static int
input_field_ok(struct dialog_data *dlg_data, struct widget_data *di)
{
	void (*fn)(void *, unsigned char *) = di->widget->udata;
	void *data = dlg_data->dlg->udata2;
	unsigned char *text = dlg_data->items->cdata;

	if (check_dialog(dlg_data)) return 1;

	add_to_input_history(dlg_data->dlg->items->history, text, 1);

	if (fn) fn(data, text);
	ok_dialog(dlg_data, di);
	return 0;
}

void
input_field_fn(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct color_pair *text_color = get_bfu_color(term, "dialog.text");

	text_width(term, dlg_data->dlg->udata, &min, &max);
	buttons_width(term, dlg_data->items + 1, 2, &min, &max);

	if (max < dlg_data->dlg->items->dlen)
		max = dlg_data->dlg->items->dlen;

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;

	rw = 0; /* !!! FIXME: input field */
	dlg_format_text(NULL, term, dlg_data->dlg->udata, 0, &y, w, &rw,
			text_color, AL_LEFT);
	dlg_format_field(NULL, term, dlg_data->items, 0, &y, w, &rw,
			 AL_LEFT);

	y++;
	dlg_format_buttons(NULL, term, dlg_data->items + 1, 2, 0, &y, w, &rw,
			   AL_CENTER);

	w = rw;
	dlg_data->xw = rw + 2 * DIALOG_LB;
	dlg_data->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg_data);

	draw_dlg(dlg_data);

	y = dlg_data->y + DIALOG_TB;
	dlg_format_text(term, term, dlg_data->dlg->udata, dlg_data->x + DIALOG_LB,
			&y, w, NULL, text_color, AL_LEFT);
	dlg_format_field(term, term, dlg_data->items, dlg_data->x + DIALOG_LB,
			 &y, w, NULL, AL_LEFT);

	y++;
	dlg_format_buttons(term, term, dlg_data->items + 1, 2, dlg_data->x + DIALOG_LB,
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
	int n = 0;

	if (intl) {
		title = _(title, term);
		text = _(text, term);
		okbutton = _(okbutton, term);
		cancelbutton = _(cancelbutton, term);
	}

#define INPUT_DLG_SIZE 3
	dlg = calloc_dialog(INPUT_DLG_SIZE, l);
	if (!dlg) return;

	field = (unsigned char *) dlg + sizeof_dialog(INPUT_DLG_SIZE, 0);
	*field = 0;

	if (def) {
		int defsize = strlen(def) + 1;

		memcpy(field, def, (defsize > l) ? l - 1 : defsize);
	}

	dlg->title = title;
	dlg->fn = input_field_fn;
	dlg->udata = text;
	dlg->udata2 = data;

	add_dlg_field(dlg, n, min, max, check, l, field, history);

	add_dlg_button(dlg, n, B_ENTER, input_field_ok, okbutton, fn);
	add_dlg_button(dlg, n, B_ESC, input_field_cancel, cancelbutton, cancelfn);

	add_dlg_end(dlg, n);

	assert(n == INPUT_DLG_SIZE);

	add_to_ml(&ml, dlg, NULL);
	do_dialog(term, dlg, ml);
}


static inline void
display_field_do(struct widget_data *di, struct dialog_data *dlg_data,
		 int sel, int hide)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color;

	int_bounds(&di->vpos, di->cpos - di->l + 1, di->cpos);
	int_lower_bound(&di->vpos, 0);

	color = get_bfu_color(term, "dialog.field");
	if (color)
		draw_area(term, di->x, di->y, di->l, 1, ' ', 0, color);

	color = get_bfu_color(term, "dialog.field-text");
	if (color) {
		int len = strlen(di->cdata + di->vpos);
		int l = int_min(len, di->l);

		if (!hide) {
			draw_text(term, di->x, di->y, di->cdata + di->vpos, l,
				  0, color);
		} else {
			draw_area(term, di->x, di->y, l, 1, '*', 0, color);
		}
	}

	if (sel) {
		int x = di->x + di->cpos - di->vpos;

		set_cursor(term, x, di->y, 0);
		set_window_ptr(dlg_data->win, di->x, di->y);
	}
}

static void
display_field(struct widget_data *di, struct dialog_data *dlg_data, int sel)
{
	display_field_do(di, dlg_data, sel, 0);
}

static void
display_field_pass(struct widget_data *di, struct dialog_data *dlg_data, int sel)
{
	display_field_do(di, dlg_data, sel, 1);
}

static void
init_field(struct widget_data *widget, struct dialog_data *dlg_data,
	   struct term_event *ev)
{
	if (widget->widget->history) {
		struct input_history_item *item;

		foreach (item, widget->widget->history->items) {
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
mouse_field(struct widget_data *di, struct dialog_data *dlg_data,
	    struct term_event *ev)
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
	int_upper_bound(&di->cpos, strlen(di->cdata));

	display_dlg_item(dlg_data, selected_widget(dlg_data), 0);
	dlg_data->selected = di - dlg_data->items;

dsp_f:
	display_dlg_item(dlg_data, di, 1);
	return EVENT_PROCESSED;
}

/* XXX: The world's best candidate for massive goto cleanup! --pasky */
static int
kbd_field(struct widget_data *di, struct dialog_data *dlg_data,
	  struct term_event *ev)
{
	struct window *win = dlg_data->win;
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

				safe_strncpy(di->cdata, clipboard, di->widget->dlen);
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

				if (cdata_len >= di->widget->dlen - 1)
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
	display_dlg_item(dlg_data, di, 1);
	redraw_from_window(dlg_data->win);
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
