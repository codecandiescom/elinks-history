/* $Id: options.h,v 1.6 2003/07/09 23:03:09 jonas Exp $ */

#ifndef EL__DIALOGS_OPTIONS_H
#define EL__DIALOGS_OPTIONS_H

#include "sched/session.h"
#include "terminal/terminal.h"

void charset_list(struct terminal *, void *, struct session *);
void terminal_options(struct terminal *, void *, struct session *);
void menu_language_list(struct terminal *, void *, struct session *);
void dlg_resize_terminal(struct terminal *, void *, struct session *);

#endif
