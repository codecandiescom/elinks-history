/* $Id: options.h,v 1.7 2004/11/22 13:27:41 zas Exp $ */

#ifndef EL__DIALOGS_OPTIONS_H
#define EL__DIALOGS_OPTIONS_H

#include "sched/session.h"
#include "terminal/terminal.h"

void charset_list(struct terminal *, void *, void *);
void terminal_options(struct terminal *, void *, struct session *);
void menu_language_list(struct terminal *, void *, void *);
void dlg_resize_terminal(struct terminal *term, void *xxx, void *xxxx);

#endif
