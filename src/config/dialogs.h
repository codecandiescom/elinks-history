/* $Id: dialogs.h,v 1.3 2002/12/13 23:31:59 pasky Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "document/session.h"
#include "lowlevel/terminal.h"

void write_config_error(struct terminal *term, unsigned char *config_file, int ret);
void menu_options_manager(struct terminal *, void *, struct session *);
void menu_keybinding_manager(struct terminal *, void *, struct session *);

#endif
