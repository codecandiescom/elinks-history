/* $Id: dialogs.h,v 1.4 2003/01/05 16:48:13 pasky Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "lowlevel/terminal.h"
#include "sched/session.h"

void write_config_error(struct terminal *term, unsigned char *config_file, int ret);
void menu_options_manager(struct terminal *, void *, struct session *);
void menu_keybinding_manager(struct terminal *, void *, struct session *);

#endif
