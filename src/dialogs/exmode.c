/* Ex-mode-like commandline support */
/* $Id: exmode.c,v 1.12 2004/01/26 06:07:28 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
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

#define EXMODE_BUFFER_SIZE 80

struct exmode_data {
	struct dialog_data *dlg_data;
	struct dialog dlg;
	unsigned char buffer[EXMODE_BUFFER_SIZE];
};

struct input_history exmode_history = {
	/* items: */	{ D_LIST_HEAD(exmode_history.entries) },
	/* size: */	0,
	/* dirty: */	0,
	/* nosave: */	0,
};

typedef int (*exmode_handler)(struct session *, unsigned char *, unsigned char *);

static int
exmode_action_handler(struct session *ses, unsigned char *command,
		      unsigned char *args)
{
	enum main_action action = read_action(KM_MAIN, command);

	if (action == ACT_MAIN_NONE) return 0;

	if (!*args)
		return do_action(ses, action, 0) == action;

	switch (action) {
		case ACT_MAIN_GOTO_URL:
			goto_url_with_hook(ses, args);
			return 1;
		default:
			break;
	}
	return 0;
}


static exmode_handler exmode_handlers[] = {
	exmode_action_handler,
	NULL,
};

static void
exmode_exec(struct exmode_data *data)
{
	/* First look it up as action, then try it as an event (but the event
	 * part should be thought out somehow yet, I s'pose... let's leave it
	 * off for now). Then try to evaluate it as configfile command. Then at
	 * least pop up an error. */
	struct session *ses = data->dlg_data->dlg->udata2;
	unsigned char *command = data->dlg_data->widgets_data->cdata;
	unsigned char *end = command;
	int i;

	if (!*command) return;

	add_to_input_history(&exmode_history, command, 1);

	while (*end && !isspace(*end)) end++;
	if (*end) *end++ = 0;

	for (i = 0; exmode_handlers[i]; i++) {
		if (exmode_handlers[i](ses, command, end))
			break;
	}
}


static void
exmode_layouter(struct dialog_data *dlg_data)
{
	struct window *win = dlg_data->win;
	struct session *ses = dlg_data->dlg->udata2;
	int y = win->term->height - 1
		- ses->status.show_status_bar
		- ses->status.show_tabs_bar;

	dlg_format_field(win->term, dlg_data->widgets_data, 0,
			 &y, win->term->width, NULL, AL_LEFT);
}

static int
exmode_handle_event(struct dialog_data *dlg_data, struct term_event *ev)
{
	struct exmode_data *data = dlg_data->dlg->udata;

	switch (ev->ev) {
		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
		case EV_MOUSE:
		case EV_ABORT:
			/* dialog_func() handles these for use */
			break;

		case EV_KBD:
			switch (kbd_action(KM_EDIT, ev, NULL)) {
				case ACT_EDIT_ENTER:
					exmode_exec(data);
					/* Falling */
				case ACT_EDIT_CANCEL:
					cancel_dialog(dlg_data, NULL);
					break;
				default:
					return EVENT_NOT_PROCESSED;
			}

			/* Let the input field handle it */
			return EVENT_PROCESSED;
	}

	return EVENT_NOT_PROCESSED;
}


void
exmode_start(struct session *ses)
{
	struct exmode_data *data;
	struct dialog_data *dlg_data;

	assert(ses);

	data = mem_calloc(1, sizeof(struct exmode_data));
	if (!data) return;

	data->dlg.handle_event = exmode_handle_event;
	data->dlg.layouter = exmode_layouter;
	data->dlg.layout.only_widgets = 1;
	data->dlg.udata = data;
	data->dlg.udata2 = ses;
	data->dlg.widgets->info.field.float_label = 1;

	add_dlg_field(&data->dlg, ":", 0, 0, NULL, 80, data->buffer, &exmode_history);

	dlg_data = do_dialog(ses->tab->term, &data->dlg, getml(data, NULL));
	if (dlg_data) data->dlg_data = dlg_data;
}
