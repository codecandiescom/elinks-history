/* $Id: dialogs.h,v 1.5 2003/05/04 17:25:52 pasky Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "terminal/terminal.h"
#include "sched/session.h"

void write_config_error(struct terminal *term, unsigned char *config_file, int ret);
void menu_options_manager(struct terminal *, void *, struct session *);
void menu_keybinding_manager(struct terminal *, void *, struct session *);

#endif
