/* $Id: dialogs.h,v 1.11 2003/11/21 23:07:28 jonas Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

extern struct hierbox_browser option_browser;
extern struct hierbox_browser keybinding_browser;

void write_config_error(struct terminal *term, struct memory_list *ml, unsigned char *config_file, unsigned char *strerr);
void menu_options_manager(struct terminal *, void *, struct session *);
void menu_keybinding_manager(struct terminal *, void *, struct session *);

#endif
