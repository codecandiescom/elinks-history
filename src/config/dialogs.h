/* $Id: dialogs.h,v 1.7 2003/06/04 20:05:55 zas Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "terminal/terminal.h"
#include "sched/session.h"

void write_config_error(struct terminal *term, struct memory_list *ml, unsigned char *config_file, unsigned char *strerr);
void menu_options_manager(struct terminal *, void *, struct session *);
void menu_keybinding_manager(struct terminal *, void *, struct session *);

#endif
