/* Ex-mode-like commandline support */
/* $Id: exmode.c,v 1.5 2004/01/25 15:28:16 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/style.h"
#include "bfu/widget.h"
#include "config/kbdbind.h"
#include "dialogs/exmode.h"
#include "intl/gettext/libintl.h"
#include "sched/action.h"
#include "sched/session.h"
#include "sched/task.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* The Ex-mode commandline is that blue-yellow thing which appears at the
 * bottom of the screen when you press ':' and lets you enter various commands
 * (just like in vi), especially actions, events (where they make sense) and
 * config-file commands. */

/* TODO: Compile-time configurable? */

/* TODO: Backspace on empty field cancels the field. And so does <Esc>, for
 * that matter. --pasky */


struct exmode_data {
	struct widget_data inpfield_data;
	struct widget inpfield;
	struct dialog_data dlg_data;
	struct session *ses;
};


static void
exmode_exec(struct exmode_data *data)
{
	/* First look it up as action, then try it as an event (but the event
	 * part should be thought out somehow yet, I s'pose... let's leave it
	 * off for now). Then try to evaluate it as configfile command. Then at
	 * least pop up an error. */
	enum main_action action;
	unsigned char *command = data->inpfield.data;
	unsigned char *end = command;
	unsigned char end_char = 0;

	while (*end && !isspace(*end)) end++;
	if (*end) {
		end_char = *end;
		*end = 0;
	}

	action = read_action(KM_MAIN, command);
	if (end_char) *end = end_char;

	if (action == ACT_MAIN_NONE) {
		/* TODO; A timed error message */
		return;

	} else if (!*end) {
		if (do_action(data->ses, action, 0) != action) {
			/* TODO; A timed error message */
		}
		return;
	}

	switch (action) {
		case ACT_MAIN_GOTO_URL:
			goto_url_with_hook(data->ses, end + 1);
			break;
		default:
			break;
	}
}

static void
exmode_display(struct window *win, struct exmode_data *data)
{
	field_ops.display(&data->inpfield_data, &data->dlg_data, 1);
	redraw_from_window(win);
}

static void
exmode_func(struct window *win, struct term_event *ev, int fwd)
{
	struct exmode_data *data = win->data;
	struct session *ses = data->ses;

	data->dlg_data.win = win;

	switch (ev->ev) {
		case EV_INIT:
			{
			int y = win->term->height - 1
				- ses->status.show_status_bar
				- ses->status.show_tabs_bar;

			field_ops.init(&data->inpfield_data, &data->dlg_data, NULL);
			dlg_format_field(win->term, &data->inpfield_data, 0,
					 &y, win->term->width, NULL, AL_LEFT);
			}
		case EV_RESIZE:
		case EV_REDRAW:
			exmode_display(win, data);
			break;

		case EV_MOUSE:
#ifdef CONFIG_MOUSE
			field_ops.mouse(&data->inpfield_data, &data->dlg_data, ev);
#endif /* CONFIG_MOUSE */
			break;

		case EV_KBD:
			field_ops.kbd(&data->inpfield_data, &data->dlg_data, ev);
			switch (kbd_action(KM_EDIT, ev, NULL)) {
				case ACT_EDIT_ENTER:
					exmode_exec(data);
					/* Falling */
				case ACT_EDIT_CANCEL:
					delete_window(win);
					break;
				default:
					break;
			}
			break;

		case EV_ABORT:
			mem_free(data->inpfield.data);
			break;
	}
}


void
exmode_start(struct session *ses)
{
	struct exmode_data *data;

	assert(ses);

	/* TODO: History */

	data = mem_calloc(1, sizeof(struct exmode_data));
	if (!data) return;

	data->ses = ses;

	data->inpfield.ops = &field_ops;
	data->inpfield.text = ":";
	data->inpfield.info.field.float_label = 1;
	data->inpfield.datalen = 80; /* Completely arbitrary. */
	data->inpfield.data = mem_alloc(81);
	if (!data->inpfield.data) {
		mem_free(data);
		return;
	}
	*data->inpfield.data = 0;

	data->inpfield_data.widget = &data->inpfield;
	data->inpfield_data.cdata = data->inpfield.data;

	add_window(ses->tab->term, exmode_func, data);
}
