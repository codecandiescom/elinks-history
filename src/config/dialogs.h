/* $Id: dialogs.h,v 1.2 2002/12/07 22:26:30 pasky Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "document/session.h"
#include "lowlevel/terminal.h"

void write_config_error(struct terminal *term, unsigned char *config_file, int ret);
void menu_options_manager(struct terminal *, void *, struct session *);

#endif
