/* $Id: dialogs.h,v 1.8 2003/07/09 23:03:09 jonas Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "sched/session.h"
#include "terminal/terminal.h"

void write_config_error(struct terminal *term, struct memory_list *ml, unsigned char *config_file, unsigned char *strerr);
void menu_options_manager(struct terminal *, void *, struct session *);
void menu_keybinding_manager(struct terminal *, void *, struct session *);

#endif
