/* $Id: options.h,v 1.5 2003/05/04 17:25:53 pasky Exp $ */

#ifndef EL__DIALOGS_OPTIONS_H
#define EL__DIALOGS_OPTIONS_H

#include "terminal/terminal.h"
#include "sched/session.h"

void charset_list(struct terminal *, void *, struct session *);
void terminal_options(struct terminal *, void *, struct session *);
void menu_language_list(struct terminal *, void *, struct session *);
void dlg_resize_terminal(struct terminal *, void *, struct session *);

#endif
