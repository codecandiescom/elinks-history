/* $Id: dialogs.h,v 1.13 2004/01/07 03:18:19 jonas Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

extern struct hierbox_browser option_browser;
extern struct hierbox_browser keybinding_browser;

void write_config_error(struct terminal *term, struct memory_list *ml, unsigned char *config_file, unsigned char *strerr);
void write_config_success(struct terminal *term, struct memory_list *ml, unsigned char *config_file);
void options_manager(struct session *);
void keybinding_manager(struct session *);

#endif
