/* Input field widget implementation. */
/* $Id: inpfield.c,v 1.85 2003/11/05 20:08:16 jonas Exp $ */

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
#include "osdep/osdep.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/memlist.h"
#include "util/memory.h"


int
check_number(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	unsigned char *end;
	long l;

	errno = 0;
	l = strtol(widget_data->cdata, (char **)&end, 10);

	if (errno || !*widget_data->cdata || *end) {
		msg_box(dlg_data->win->term, NULL, 0,
			N_("Bad number"), AL_CENTER,
			N_("Number expected in field"),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		return 1;
	}

	if (l < widget_data->widget->info.field.min || l > widget_data->widget->info.field.max) {
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
check_nonempty(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	unsigned char *p;

	for (p = widget_data->cdata; *p; p++)
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
		 struct widget_data *widget_data,
		 int x, int *y, int w, int *rw, enum format_align align)
{
	widget_data->x = x;
	widget_data->y = *y;
	widget_data->w = w;

	if (rw && w > *rw) *rw = w;
	(*y)++;
}

static int
input_field_cancel(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	void (*fn)(void *) = widget_data->widget->udata;
	void *data = dlg_data->dlg->udata2;

	if (fn) fn(data);
	cancel_dialog(dlg_data, widget_data);

	return 0;
}

static int
input_field_ok(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	void (*fn)(void *, unsigned char *) = widget_data->widget->udata;
	void *data = dlg_data->dlg->udata2;
	unsigned char *text = dlg_data->widgets_data->cdata;

	if (check_dialog(dlg_data)) return 1;

	if (dlg_data->dlg->widgets->info.field.history)
		add_to_input_history(dlg_data->dlg->widgets->info.field.history,
				     text, 1);

	if (fn) fn(data, text);
	ok_dialog(dlg_data, widget_data);
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
	buttons_width(dlg_data->widgets_data + 1, 2, &min, &max);

	if (max < dlg_data->dlg->widgets->datalen)
		max = dlg_data->dlg->widgets->datalen;

	w = term->width * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;

	rw = 0; /* !!! FIXME: input field */
	dlg_format_text(NULL, term, dlg_data->dlg->udata, 0, &y, w, &rw,
			text_color, AL_LEFT);
	dlg_format_field(NULL, term, dlg_data->widgets_data, 0, &y, w, &rw,
			 AL_LEFT);

	y++;
	dlg_format_buttons(NULL, dlg_data->widgets_data + 1, 2, 0, &y, w, &rw,
			   AL_CENTER);

	w = rw;
	dlg_data->width = rw + 2 * DIALOG_LB;
	dlg_data->height = y + 2 * DIALOG_TB;
	center_dlg(dlg_data);

	draw_dlg(dlg_data);

	y = dlg_data->y + DIALOG_TB;
	dlg_format_text(term, term, dlg_data->dlg->udata, dlg_data->x + DIALOG_LB,
			&y, w, NULL, text_color, AL_LEFT);
	dlg_format_field(term, term, dlg_data->widgets_data, dlg_data->x + DIALOG_LB,
			 &y, w, NULL, AL_LEFT);

	y++;
	dlg_format_buttons(term, dlg_data->widgets_data + 1, 2, dlg_data->x + DIALOG_LB,
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

#define INPUT_WIDGETS_COUNT 3
	dlg = calloc_dialog(INPUT_WIDGETS_COUNT, l);
	if (!dlg) return;

	field = (unsigned char *) dlg + sizeof_dialog(INPUT_WIDGETS_COUNT, 0);
	/* *field = 0; */ /* calloc() job. --Zas */

	if (def) {
		int defsize = strlen(def) + 1;

		memcpy(field, def, (defsize > l) ? l - 1 : defsize);
	}

	dlg->title = title;
	dlg->fn = input_field_fn;
	dlg->udata = text;
	dlg->udata2 = data;

	add_dlg_field(dlg, min, max, check, l, field, history);

	add_dlg_button(dlg, B_ENTER, input_field_ok, okbutton, fn);
	add_dlg_button(dlg, B_ESC, input_field_cancel, cancelbutton, cancelfn);

	add_dlg_end(dlg, INPUT_WIDGETS_COUNT);

	add_to_ml(&ml, dlg, NULL);
	do_dialog(term, dlg, ml);
}


static inline void
display_field_do(struct widget_data *widget_data, struct dialog_data *dlg_data,
		 int sel, int hide)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color;

	int_bounds(&widget_data->info.field.vpos,
		   widget_data->info.field.cpos - widget_data->w + 1,
		   widget_data->info.field.cpos);
	int_lower_bound(&widget_data->info.field.vpos, 0);

	color = get_bfu_color(term, "dialog.field");
	if (color)
		draw_area(term, widget_data->x, widget_data->y, widget_data->w, 1, ' ', 0, color);

	color = get_bfu_color(term, "dialog.field-text");
	if (color) {
		int len = strlen(widget_data->cdata + widget_data->info.field.vpos);
		int w = int_min(len, widget_data->w);

		if (!hide) {
			draw_text(term, widget_data->x, widget_data->y,
				  widget_data->cdata + widget_data->info.field.vpos, w,
				  0, color);
		} else {
			draw_area(term, widget_data->x, widget_data->y, w, 1, '*', 0, color);
		}
	}

	if (sel) {
		int x = widget_data->x + widget_data->info.field.cpos - widget_data->info.field.vpos;

		set_cursor(term, x, widget_data->y, 0);
		set_window_ptr(dlg_data->win, widget_data->x, widget_data->y);
	}
}

static void
display_field(struct widget_data *widget_data, struct dialog_data *dlg_data, int sel)
{
	display_field_do(widget_data, dlg_data, sel, 0);
}

static void
display_field_pass(struct widget_data *widget_data, struct dialog_data *dlg_data, int sel)
{
	display_field_do(widget_data, dlg_data, sel, 1);
}

static void
init_field(struct widget_data *widget_data, struct dialog_data *dlg_data,
	   struct term_event *ev)
{
	if (widget_has_history(widget_data)) {
		struct input_history_entry *entry;

		foreach (entry, widget_data->widget->info.field.history->entries) {
			int datalen = strlen(entry->data);
			struct input_history_entry *new_entry;

			/* One byte is reserved in struct input_history_entry. */
			new_entry = mem_alloc(sizeof(struct input_history_entry)
					      + datalen);
			if (!new_entry) continue;

			memcpy(new_entry->data, entry->data, datalen + 1);
			add_to_list(widget_data->info.field.history, new_entry);
		}
	}

	widget_data->info.field.cpos = strlen(widget_data->cdata);
}

static int
mouse_field(struct widget_data *widget_data, struct dialog_data *dlg_data,
	    struct term_event *ev)
{
	if (ev->y != widget_data->y || ev->x < widget_data->x
	    || ev->x >= widget_data->x + widget_data->w
	    || !widget_has_history(widget_data))
		return EVENT_NOT_PROCESSED;

	switch (ev->b & BM_BUTT) {
		case B_WHEEL_UP:
			if ((ev->b & BM_ACT) == B_DOWN &&
			    (void *) widget_data->info.field.cur_hist->prev != &widget_data->info.field.history) {
				widget_data->info.field.cur_hist = widget_data->info.field.cur_hist->prev;
				dlg_set_history(widget_data);
				goto dsp_f;
			}
			return EVENT_PROCESSED;

		case B_WHEEL_DOWN:
			if ((ev->b & BM_ACT) == B_DOWN &&
			    (void *) widget_data->info.field.cur_hist != &widget_data->info.field.history) {
				widget_data->info.field.cur_hist = widget_data->info.field.cur_hist->next;
				dlg_set_history(widget_data);
				goto dsp_f;
			}
			return EVENT_PROCESSED;
	}

	widget_data->info.field.cpos = widget_data->info.field.vpos + ev->x - widget_data->x;
	int_upper_bound(&widget_data->info.field.cpos, strlen(widget_data->cdata));

	display_dlg_item(dlg_data, selected_widget(dlg_data), 0);
	dlg_data->selected = widget_data - dlg_data->widgets_data;

dsp_f:
	display_dlg_item(dlg_data, widget_data, 1);
	return EVENT_PROCESSED;
}

/* XXX: The world's best candidate for massive goto cleanup! --pasky */
static int
kbd_field(struct widget_data *widget_data, struct dialog_data *dlg_data,
	  struct term_event *ev)
{
	struct window *win = dlg_data->win;
	struct terminal *term = win->term;

	switch (kbd_action(KM_EDIT, ev, NULL)) {
		case ACT_UP:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			if ((void *) widget_data->info.field.cur_hist->prev != &widget_data->info.field.history) {
				widget_data->info.field.cur_hist = widget_data->info.field.cur_hist->prev;
				dlg_set_history(widget_data);
				goto dsp_f;
			}
			break;

		case ACT_DOWN:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			if ((void *) widget_data->info.field.cur_hist != &widget_data->info.field.history) {
				widget_data->info.field.cur_hist = widget_data->info.field.cur_hist->next;
				dlg_set_history(widget_data);
				goto dsp_f;
			}
			break;

		case ACT_RIGHT:
			if (widget_data->info.field.cpos < strlen(widget_data->cdata))
				widget_data->info.field.cpos++;
			goto dsp_f;

		case ACT_LEFT:
			if (widget_data->info.field.cpos > 0)
				widget_data->info.field.cpos--;
			goto dsp_f;

		case ACT_HOME:
			widget_data->info.field.cpos = 0;
			goto dsp_f;

		case ACT_END:
			widget_data->info.field.cpos = strlen(widget_data->cdata);
			goto dsp_f;

		case ACT_BACKSPACE:
			if (widget_data->info.field.cpos) {
				memmove(widget_data->cdata + widget_data->info.field.cpos - 1,
					widget_data->cdata + widget_data->info.field.cpos,
					strlen(widget_data->cdata) - widget_data->info.field.cpos + 1);
				widget_data->info.field.cpos--;
			}
			goto dsp_f;

		case ACT_DELETE:
			{
				int cdata_len = strlen(widget_data->cdata);

				if (widget_data->info.field.cpos >= cdata_len) goto dsp_f;

				memmove(widget_data->cdata + widget_data->info.field.cpos,
					widget_data->cdata + widget_data->info.field.cpos + 1,
					cdata_len - widget_data->info.field.cpos + 1);
				goto dsp_f;
			}

		case ACT_KILL_TO_BOL:
			memmove(widget_data->cdata,
					widget_data->cdata + widget_data->info.field.cpos,
					strlen(widget_data->cdata + widget_data->info.field.cpos) + 1);
			widget_data->info.field.cpos = 0;
			goto dsp_f;

		case ACT_KILL_TO_EOL:
			widget_data->cdata[widget_data->info.field.cpos] = 0;
			goto dsp_f;

		case ACT_COPY_CLIPBOARD:
			/* Copy to clipboard */
			set_clipboard_text(widget_data->cdata);
			break;	/* We don't need to redraw */

		case ACT_CUT_CLIPBOARD:
			/* Cut to clipboard */
			set_clipboard_text(widget_data->cdata);
			widget_data->cdata[0] = 0;
			widget_data->info.field.cpos = 0;
			goto dsp_f;

		case ACT_PASTE_CLIPBOARD:
			{
				/* Paste from clipboard */
				unsigned char *clipboard = get_clipboard_text();

				if (!clipboard) goto dsp_f;

				safe_strncpy(widget_data->cdata, clipboard, widget_data->widget->datalen);
				widget_data->info.field.cpos = strlen(widget_data->cdata);
				mem_free(clipboard);
				goto dsp_f;
			}

		case ACT_AUTO_COMPLETE:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			do_tab_compl(term, &widget_data->info.field.history, win);
			goto dsp_f;

		case ACT_AUTO_COMPLETE_UNAMBIGUOUS:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			do_tab_compl_unambiguous(term, &widget_data->info.field.history, win);
			goto dsp_f;

		default:
			if (ev->x >= ' ' && ev->x < 0x100 && !ev->y) {
				int cdata_len = strlen(widget_data->cdata);

				if (cdata_len >= widget_data->widget->datalen - 1)
					goto dsp_f;

				memmove(widget_data->cdata + widget_data->info.field.cpos + 1,
					widget_data->cdata + widget_data->info.field.cpos,
					cdata_len - widget_data->info.field.cpos + 1);
				widget_data->cdata[widget_data->info.field.cpos++] = ev->x;
				goto dsp_f;
			}
	}
	return EVENT_NOT_PROCESSED;

dsp_f:
	display_dlg_item(dlg_data, widget_data, 1);
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
