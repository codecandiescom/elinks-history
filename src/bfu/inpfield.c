/* Input field widget ismplementation. */
/* $Id: inpfield.c,v 1.176 2004/11/18 00:52:43 zas Exp $ */

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
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/memlist.h"
#include "util/memory.h"


t_handler_event_status
check_number(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct widget *widget = widget_data->widget;
	unsigned char *end;
	long l;

	errno = 0;
	l = strtol(widget_data->cdata, (char **) &end, 10);

	if (errno || !*widget_data->cdata || *end) {
		msg_box(dlg_data->win->term, NULL, 0,
			N_("Bad number"), ALIGN_CENTER,
			N_("Number expected in field"),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		return EVENT_NOT_PROCESSED;
	}

	if (l < widget->info.field.min || l > widget->info.field.max) {
		msg_box(dlg_data->win->term, NULL, MSGBOX_FREE_TEXT,
			N_("Bad number"), ALIGN_CENTER,
			msg_text(dlg_data->win->term,
				 N_("Number should be in the range from %d to %d."),
				 widget->info.field.min, widget->info.field.max),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		return EVENT_NOT_PROCESSED;
	}

	return EVENT_PROCESSED;
}

t_handler_event_status
check_nonempty(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	unsigned char *p;

	for (p = widget_data->cdata; *p; p++)
		if (*p > ' ')
			return EVENT_PROCESSED;

	msg_box(dlg_data->win->term, NULL, 0,
		N_("Bad string"), ALIGN_CENTER,
		N_("Empty string not allowed"),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);

	return EVENT_NOT_PROCESSED;
}

void
dlg_format_field(struct terminal *term,
		 struct widget_data *widget_data,
		 int x, int *y, int w, int *rw, enum format_align align)
{
	static int max_label_width;
	static int *prev_y; /* Assert the uniqueness of y */
	unsigned char *label = widget_data->widget->text;
	struct color_pair *text_color = NULL;
	int label_width = 0;

	if (widget_data->widget->info.field.float_label && label) {
		label_width = strlen(label);
		if (prev_y == y) {
			int_lower_bound(&max_label_width, label_width);
		} else {
			max_label_width = label_width;
			prev_y = y;
		}

		/* Right align the floating label up against the
		 * input field */
		x += max_label_width - label_width;
		w -= max_label_width - label_width;
	}

	if (label) {
		if (term) text_color = get_bfu_color(term, "dialog.text");

		dlg_format_text_do(term, label, x, y, w, rw, text_color, ALIGN_LEFT);
	}

	/* XXX: We want the field and label on the same line if the terminal
	 * width allows it. */
	if (widget_data->widget->info.field.float_label && label) {
		if (widget_data->widget->info.field.float_label != 2) {
			(*y)--;
			dlg_format_text_do(term, ":", x + label_width, y, w, rw, text_color, ALIGN_LEFT);
			w -= 2;
			x += 2;
		}

		/* FIXME: Is 5 chars for input field enough? --jonas */
		if (label_width < w - 5) {
			(*y)--;
			w -= label_width;
			x += label_width;
		}
	}

	if (rw) int_lower_bound(rw, int_min(w, DIALOG_MIN_WIDTH));

	set_box(&widget_data->box, x, *y, w, 1);

	(*y)++;
}

static t_handler_event_status
input_field_cancel(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	void (*fn)(void *) = widget_data->widget->udata;
	void *data = dlg_data->dlg->udata2;

	if (fn) fn(data);
	cancel_dialog(dlg_data, widget_data);

	return EVENT_PROCESSED;
}

static t_handler_event_status
input_field_ok(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	void (*fn)(void *, unsigned char *) = widget_data->widget->udata;
	void *data = dlg_data->dlg->udata2;
	unsigned char *text = dlg_data->widgets_data->cdata;

	if (check_dialog(dlg_data)) return EVENT_NOT_PROCESSED;

	if (widget_has_history(dlg_data->widgets_data))
		add_to_input_history(dlg_data->dlg->widgets->info.field.history,
				     text, 1);

	if (fn) fn(data, text);

	return cancel_dialog(dlg_data, widget_data);
}

void
input_field(struct terminal *term, struct memory_list *ml, int intl,
	    unsigned char *title,
	    unsigned char *text,
	    unsigned char *okbutton,
	    unsigned char *cancelbutton,
	    void *data, struct input_history *history, int l,
	    unsigned char *def, int min, int max,
	    t_handler_event_status (*check)(struct dialog_data *, struct widget_data *),
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

	/* @field is automatically cleared by calloc() */
	field = get_dialog_offset(dlg, INPUT_WIDGETS_COUNT);

	if (def) {
		int defsize = strlen(def) + 1;

		memcpy(field, def, (defsize > l) ? l - 1 : defsize);
	}

	dlg->title = title;
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.fit_datalen = 1;
	dlg->udata2 = data;

	add_dlg_field(dlg, text, min, max, check, l, field, history);

	add_dlg_button(dlg, B_ENTER, input_field_ok, okbutton, fn);
	add_dlg_button(dlg, B_ESC, input_field_cancel, cancelbutton, cancelfn);

	add_dlg_end(dlg, INPUT_WIDGETS_COUNT);

	add_to_ml(&ml, dlg, NULL);
	do_dialog(term, dlg, ml);
}

static t_handler_event_status
display_field_do(struct dialog_data *dlg_data, struct widget_data *widget_data,
		 int hide)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color;
	int sel = dlg_data->focus_selected_widget;

	int_bounds(&widget_data->info.field.vpos,
		   widget_data->info.field.cpos - widget_data->box.width + 1,
		   widget_data->info.field.cpos);
	int_lower_bound(&widget_data->info.field.vpos, 0);

	color = get_bfu_color(term, "dialog.field");
	if (color)
		draw_box(term, &widget_data->box, ' ', 0, color);

	color = get_bfu_color(term, "dialog.field-text");
	if (color) {
		int len = strlen(widget_data->cdata + widget_data->info.field.vpos);
		int w = int_min(len, widget_data->box.width);

		if (!hide) {
			draw_text(term, widget_data->box.x, widget_data->box.y,
				  widget_data->cdata + widget_data->info.field.vpos, w,
				  0, color);
		} else {
			struct box box;

			copy_box(&box, &widget_data->box);
			box.width = w;

			draw_box(term, &box, '*', 0, color);
		}
	}

	if (sel) {
		int x = widget_data->box.x + widget_data->info.field.cpos - widget_data->info.field.vpos;

		set_cursor(term, x, widget_data->box.y, 0);
		set_window_ptr(dlg_data->win, widget_data->box.x, widget_data->box.y);
	}

	return EVENT_PROCESSED;
}

static t_handler_event_status
display_field(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	return display_field_do(dlg_data, widget_data, 0);
}

static t_handler_event_status
display_field_pass(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	return display_field_do(dlg_data, widget_data, 1);
}

static t_handler_event_status
init_field(struct dialog_data *dlg_data, struct widget_data *widget_data)
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
	return EVENT_PROCESSED;
}

static t_handler_event_status
mouse_field(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct term_event *ev = dlg_data->term_event;

	if (!widget_has_history(widget_data))
		return EVENT_NOT_PROCESSED;

	if (!check_mouse_position(ev, &widget_data->box))
		return EVENT_NOT_PROCESSED;

	switch (get_mouse_button(ev)) {
		case B_WHEEL_UP:
			if (check_mouse_action(ev, B_DOWN) &&
			    (void *) widget_data->info.field.cur_hist->prev != &widget_data->info.field.history) {
				widget_data->info.field.cur_hist = widget_data->info.field.cur_hist->prev;
				dlg_set_history(widget_data);
				goto display_field;
			}
			return EVENT_PROCESSED;

		case B_WHEEL_DOWN:
			if (check_mouse_action(ev, B_DOWN) &&
			    (void *) widget_data->info.field.cur_hist != &widget_data->info.field.history) {
				widget_data->info.field.cur_hist = widget_data->info.field.cur_hist->next;
				dlg_set_history(widget_data);
				goto display_field;
			}
			return EVENT_PROCESSED;
	}

	widget_data->info.field.cpos = widget_data->info.field.vpos
				     + ev->info.mouse.x - widget_data->box.x;
	int_upper_bound(&widget_data->info.field.cpos, strlen(widget_data->cdata));

	display_widget_unfocused(dlg_data, selected_widget(dlg_data));
	dlg_data->selected = widget_data - dlg_data->widgets_data;

display_field:
	display_widget_focused(dlg_data, widget_data);
	return EVENT_PROCESSED;
}

static t_handler_event_status
kbd_field(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct window *win = dlg_data->win;
	struct terminal *term = win->term;
	struct term_event *ev = dlg_data->term_event;
	
	switch (kbd_action(KEYMAP_EDIT, ev, NULL)) {
		case ACT_EDIT_UP:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			if ((void *) widget_data->info.field.cur_hist->prev != &widget_data->info.field.history) {
				widget_data->info.field.cur_hist = widget_data->info.field.cur_hist->prev;
				dlg_set_history(widget_data);
				goto display_field;
			}
			break;

		case ACT_EDIT_DOWN:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			if ((void *) widget_data->info.field.cur_hist != &widget_data->info.field.history) {
				widget_data->info.field.cur_hist = widget_data->info.field.cur_hist->next;
				dlg_set_history(widget_data);
				goto display_field;
			}
			break;

		case ACT_EDIT_RIGHT:
			if (widget_data->info.field.cpos < strlen(widget_data->cdata))
				widget_data->info.field.cpos++;
			goto display_field;

		case ACT_EDIT_LEFT:
			if (widget_data->info.field.cpos > 0)
				widget_data->info.field.cpos--;
			goto display_field;

		case ACT_EDIT_HOME:
			widget_data->info.field.cpos = 0;
			goto display_field;

		case ACT_EDIT_END:
			widget_data->info.field.cpos = strlen(widget_data->cdata);
			goto display_field;

		case ACT_EDIT_BACKSPACE:
			if (widget_data->info.field.cpos) {
				memmove(widget_data->cdata + widget_data->info.field.cpos - 1,
					widget_data->cdata + widget_data->info.field.cpos,
					strlen(widget_data->cdata) - widget_data->info.field.cpos + 1);
				widget_data->info.field.cpos--;
			}
			goto display_field;

		case ACT_EDIT_DELETE:
			{
				int cdata_len = strlen(widget_data->cdata);

				if (widget_data->info.field.cpos >= cdata_len) goto display_field;

				memmove(widget_data->cdata + widget_data->info.field.cpos,
					widget_data->cdata + widget_data->info.field.cpos + 1,
					cdata_len - widget_data->info.field.cpos + 1);
				goto display_field;
			}

		case ACT_EDIT_KILL_TO_BOL:
			memmove(widget_data->cdata,
					widget_data->cdata + widget_data->info.field.cpos,
					strlen(widget_data->cdata + widget_data->info.field.cpos) + 1);
			widget_data->info.field.cpos = 0;
			goto display_field;

		case ACT_EDIT_KILL_TO_EOL:
			widget_data->cdata[widget_data->info.field.cpos] = 0;
			goto display_field;

		case ACT_EDIT_COPY_CLIPBOARD:
			/* Copy to clipboard */
			set_clipboard_text(widget_data->cdata);
			return EVENT_PROCESSED;

		case ACT_EDIT_CUT_CLIPBOARD:
			/* Cut to clipboard */
			set_clipboard_text(widget_data->cdata);
			widget_data->cdata[0] = 0;
			widget_data->info.field.cpos = 0;
			goto display_field;

		case ACT_EDIT_PASTE_CLIPBOARD:
			{
				/* Paste from clipboard */
				unsigned char *clipboard = get_clipboard_text();

				if (!clipboard) goto display_field;

				safe_strncpy(widget_data->cdata, clipboard, widget_data->widget->datalen);
				widget_data->info.field.cpos = strlen(widget_data->cdata);
				mem_free(clipboard);
				goto display_field;
			}

		case ACT_EDIT_AUTO_COMPLETE:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			do_tab_compl(dlg_data, &widget_data->info.field.history);
			goto display_field;

		case ACT_EDIT_AUTO_COMPLETE_UNAMBIGUOUS:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			do_tab_compl_unambiguous(dlg_data, &widget_data->info.field.history);
			goto display_field;

		case ACT_EDIT_REDRAW:
			redraw_terminal_cls(term);
			return EVENT_PROCESSED;

		default:
			if (check_kbd_textinput_key(ev)) {
				unsigned char *text = widget_data->cdata;
				int textlen = strlen(text);

				if (textlen >= widget_data->widget->datalen - 1)
					goto display_field;

				/* Shift to position of the cursor */
				textlen -= widget_data->info.field.cpos;
				text	+= widget_data->info.field.cpos++;

				memmove(text + 1, text, textlen + 1);
				*text = get_kbd_key(ev);

				goto display_field;
			}
	}
	return EVENT_NOT_PROCESSED;

display_field:
	display_widget_focused(dlg_data, widget_data);
	redraw_from_window(dlg_data->win);
	return EVENT_PROCESSED;
}

struct widget_ops field_ops = {
	display_field,
	init_field,
	mouse_field,
	kbd_field,
	NULL,
};

struct widget_ops field_pass_ops = {
	display_field_pass,
	init_field,
	mouse_field,
	kbd_field,
	NULL,
};


/* Input lines */

static void
input_line_layouter(struct dialog_data *dlg_data)
{
	struct input_line *input_line = dlg_data->dlg->udata;
	struct session *ses = input_line->ses;
	struct window *win = dlg_data->win;
	int y = win->term->height - 1
		- ses->status.show_status_bar
		- ses->status.show_tabs_bar;

	dlg_format_field(win->term, dlg_data->widgets_data, 0,
			 &y, win->term->width, NULL, ALIGN_LEFT);
}

static t_handler_event_status
input_line_event_handler(struct dialog_data *dlg_data)
{
	struct input_line *input_line = dlg_data->dlg->udata;
	input_line_handler handler = input_line->handler;
	enum edit_action action;
	struct widget_data *widget_data = dlg_data->widgets_data;
	struct term_event *ev = dlg_data->term_event;
	
	/* Noodle time */
	switch (ev->ev) {
	case EVENT_KBD:
		action = kbd_action(KEYMAP_EDIT, ev, NULL);

		/* Handle some basic actions such as quiting for empty buffers */
		switch (action) {
		case ACT_EDIT_ENTER:
		case ACT_EDIT_NEXT_ITEM:
		case ACT_EDIT_PREVIOUS_ITEM:
			if (widget_has_history(widget_data))
				add_to_input_history(widget_data->widget->info.field.history,
						     input_line->buffer, 1);
			break;

		case ACT_EDIT_BACKSPACE:
			if (!*input_line->buffer)
				goto cancel_input_line;
			break;

		case ACT_EDIT_CANCEL:
			goto cancel_input_line;

		default:
			break;
		}

		/* First let the input field do its business */
		kbd_field(dlg_data, widget_data);
		break;

	case EVENT_REDRAW:
		/* Try to catch the redraw event initiated by the history
		 * completion and only respond if something was actually
		 * updated. Meaning we have new data in the line buffer that
		 * should be propagated to the line handler. */
		if (!widget_has_history(widget_data)
		    || widget_data->info.field.cpos <= 0
		    || widget_data->info.field.cpos <= strlen(input_line->buffer))
			return EVENT_NOT_PROCESSED;

		/* Fall thru */

	case EVENT_RESIZE:
		action = ACT_EDIT_REDRAW;
		break;

	default:
		return EVENT_NOT_PROCESSED;
	}

	update_dialog_data(dlg_data, widget_data);

send_action_to_handler:
	/* Then pass it on to the specialized handler */
	switch (handler(input_line, action)) {
	case INPUT_LINE_CANCEL:
cancel_input_line:
		cancel_dialog(dlg_data, widget_data);
		break;

	case INPUT_LINE_REWIND:
		/* This is stolen kbd_field() handling for ACT_EDIT_BACKSPACE */
		memmove(widget_data->cdata + widget_data->info.field.cpos - 1,
			widget_data->cdata + widget_data->info.field.cpos,
			strlen(widget_data->cdata) - widget_data->info.field.cpos + 1);
		widget_data->info.field.cpos--;

		update_dialog_data(dlg_data, widget_data);

		goto send_action_to_handler;

	case INPUT_LINE_PROCEED:
		break;
	}

	/* Hack: We want our caller to perform its redrawing routine,
	 * even if we did process the event here. */
	if (action == ACT_EDIT_REDRAW) return EVENT_NOT_PROCESSED;

	/* Completely bypass any further dialog event handling */
	return EVENT_PROCESSED;
}

void
input_field_line(struct session *ses, unsigned char *prompt, void *data,
		 struct input_history *history, input_line_handler handler)
{
	struct dialog *dlg;
	unsigned char *buffer;
	struct input_line *input_line;

	assert(ses);

	dlg = calloc_dialog(INPUT_LINE_WIDGETS, sizeof(struct input_line));
	if (!dlg) return;

	input_line = (void *) get_dialog_offset(dlg, INPUT_LINE_WIDGETS);
	input_line->ses = ses;
	input_line->data = data;
	input_line->handler = handler;
	buffer = input_line->buffer;

	dlg->handle_event = input_line_event_handler;
	dlg->layouter = input_line_layouter;
	dlg->layout.only_widgets = 1;
	dlg->udata = input_line;
	dlg->widgets->info.field.float_label = 2;

	add_dlg_field(dlg, prompt, 0, 0, NULL, INPUT_LINE_BUFFER_SIZE,
		      buffer, history);

	do_dialog(ses->tab->term, dlg, getml(dlg, NULL));
}
