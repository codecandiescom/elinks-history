/* Ex-mode-like commandline support */
/* $Id: exmode.c,v 1.38 2004/02/04 23:13:57 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_EXMODE

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/style.h"
#include "bfu/widget.h"
#include "config/conf.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "dialogs/exmode.h"
#include "intl/gettext/libintl.h"
#include "protocol/rewrite/rewrite.h"
#include "sched/action.h"
#include "sched/session.h"
#include "sched/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* The Ex-mode commandline is that blue-yellow thing which appears at the
 * bottom of the screen when you press ':' and lets you enter various commands
 * (just like in vi), especially actions, events (where they make sense) and
 * config-file commands. */

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
	if (action == ACT_MAIN_QUIT) action = ACT_MAIN_REALLY_QUIT;

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

static int
exmode_confcmd_handler(struct session *ses, unsigned char *command,
			unsigned char *args)
{
	int dummyline = 0;
	enum parse_error err;

	assert(ses && command && args);

	/* Undo the arguments separation. */
	if (*args) *(--args) = ' ';

	err = parse_config_command(config_options, &command, &dummyline, NULL);
	return err;
}

#ifdef CONFIG_URI_REWRITE
static int
exmode_uri_rewrite_handler(struct session *ses, unsigned char *command,
			   unsigned char *args)
{
	enum { CURRENT_TAB, NEW_TAB, BACKGROUNDED_TAB } open_mode = CURRENT_TAB;
	unsigned char *url = NULL;
	unsigned char *last_arg;
	unsigned char saved_arg = 0;

	/* Look for opening control chars */
	if (*args) {
		last_arg = args + strlen(args) - 1;
	} else {
		/* Check for control char in the command */
		last_arg = command + strlen(command) - 1;
	}

	/* TODO: Maybe some '<' and '>' prefixes to control where the tab is
	 * opened .. and if anyone have better ideas for control chars here
	 * please change them. --jonas */
	switch (*last_arg) {
		case '&':
			open_mode = BACKGROUNDED_TAB;
			break;
		case '+':
			open_mode = NEW_TAB;
			break;
		default:
			break;
	}

	/* Remove the control char */
	if (open_mode != CURRENT_TAB) {
		saved_arg = *last_arg;
		*last_arg = 0;
	}

	if (*args) {
		url = get_uri_rewrite_prefix(URI_REWRITE_SMART, command);
	}

	if (!url && !*args)
		url = get_uri_rewrite_prefix(URI_REWRITE_DUMB, command);

	/* Restore control char */
	if (saved_arg) *last_arg = saved_arg;

	if (!url) return 0;

	url = rewrite_uri(url, cur_loc(ses)->vs.url, args);
	if (url) {
		if (open_mode == CURRENT_TAB) {
			goto_url(ses, url);
		} else {
			int in_background = open_mode == BACKGROUNDED_TAB;

			open_url_in_new_tab(ses, url, in_background);
		}
		mem_free(url);
	}
	return !!url;
}
#endif

static exmode_handler exmode_handlers[] = {
	exmode_action_handler,
	exmode_confcmd_handler,
#ifdef CONFIG_URI_REWRITE
	exmode_uri_rewrite_handler,
#endif
	NULL,
};

static void
exmode_exec(struct session *ses, unsigned char buffer[INPUT_LINE_BUFFER_SIZE])
{
	/* First look it up as action, then try it as an event (but the event
	 * part should be thought out somehow yet, I s'pose... let's leave it
	 * off for now). Then try to evaluate it as configfile command. Then at
	 * least pop up an error. */
	unsigned char *command = buffer;
	unsigned char *args = command;
	int i;

	while (*command == ':') command++;

	if (!*command) return;

	add_to_input_history(&exmode_history, command, 1);

	while (*args && !isspace(*args)) args++;
	if (*args) *args++ = 0;

	for (i = 0; exmode_handlers[i]; i++) {
		if (exmode_handlers[i](ses, command, args))
			break;
	}
}


static enum input_line_code
exmode_input_handler(struct input_line *input_line, int action)
{
	switch (action) {
		case ACT_EDIT_ENTER:
			exmode_exec(input_line->ses, input_line->buffer);
			return INPUT_LINE_CANCEL;

		default:
			return INPUT_LINE_PROCEED;
	}
}

void
exmode_start(struct session *ses)
{
	input_field_line(ses, ":", NULL, &exmode_history, exmode_input_handler);
}

#endif /* CONFIG_EXMODE */
