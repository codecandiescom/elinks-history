/* $Id: dialogs.h,v 1.15 2004/04/11 20:41:13 jonas Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

extern struct hierbox_browser option_browser;
extern struct hierbox_browser keybinding_browser;

void write_config_error(struct terminal *term, unsigned char *config_file,
			int secsave_error, int stdio_error);
void write_config_success(struct terminal *term, unsigned char *config_file);
void options_manager(struct session *);
void keybinding_manager(struct session *);

#endif
