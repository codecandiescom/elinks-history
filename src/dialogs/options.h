/* $Id: options.h,v 1.4 2003/01/05 16:48:13 pasky Exp $ */

#ifndef EL__DIALOGS_OPTIONS_H
#define EL__DIALOGS_OPTIONS_H

#include "lowlevel/terminal.h"
#include "sched/session.h"

void charset_list(struct terminal *, void *, struct session *);
void terminal_options(struct terminal *, void *, struct session *);
void menu_language_list(struct terminal *, void *, struct session *);
void dlg_resize_terminal(struct terminal *, void *, struct session *);

#endif
