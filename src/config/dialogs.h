/* $Id: dialogs.h,v 1.6 2003/06/04 16:27:24 zas Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "terminal/terminal.h"
#include "sched/session.h"

void write_config_error(struct terminal *term, unsigned char *config_file, unsigned char *strerr);
void menu_options_manager(struct terminal *, void *, struct session *);
void menu_keybinding_manager(struct terminal *, void *, struct session *);

#endif
