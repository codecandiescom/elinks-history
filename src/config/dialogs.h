/* $Id: dialogs.h,v 1.18 2004/05/31 03:27:06 jonas Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

extern struct hierbox_browser option_browser;
extern struct hierbox_browser keybinding_browser;

extern int keybinding_text_toggle;

void write_config_dialog(struct terminal *term, unsigned char *config_file,
			 int secsave_error, int stdio_error);
void options_manager(struct session *);
void keybinding_manager(struct session *);

#endif
