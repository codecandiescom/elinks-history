/* $Id: dialogs.h,v 1.22 2005/06/10 03:57:52 miciah Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "bfu/hierbox.h"
#include "config/kbdbind.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

extern struct hierbox_browser option_browser;
extern struct hierbox_browser keybinding_browser;

void write_config_dialog(struct terminal *term, unsigned char *config_file,
			 int secsave_error, int stdio_error);
void options_manager(struct session *);
void keybinding_manager(struct session *);

struct listbox_item *get_keybinding_action_box_item(enum keymap_id km, int action);
void init_keybinding_listboxes(struct action *keymaps, struct action *actions[]);
void done_keybinding_listboxes(void);

#endif
