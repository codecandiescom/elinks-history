/* $Id: options.h,v 1.3 2002/12/11 15:21:08 pasky Exp $ */

#ifndef EL__DIALOGS_OPTIONS_H
#define EL__DIALOGS_OPTIONS_H

#include "document/session.h"
#include "lowlevel/terminal.h"

void charset_list(struct terminal *, void *, struct session *);
void terminal_options(struct terminal *, void *, struct session *);
void menu_language_list(struct terminal *, void *, struct session *);
void dlg_resize_terminal(struct terminal *, void *, struct session *);

#endif
