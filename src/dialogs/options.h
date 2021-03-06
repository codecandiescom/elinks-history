/* $Id: options.h,v 1.9 2005/06/14 12:25:20 jonas Exp $ */

#ifndef EL__DIALOGS_OPTIONS_H
#define EL__DIALOGS_OPTIONS_H

#include "session/session.h"
#include "terminal/terminal.h"

void charset_list(struct terminal *, void *, void *);
void terminal_options(struct terminal *, void *, struct session *);
void menu_language_list(struct terminal *, void *, void *);
void resize_terminal_dialog(struct terminal *term);

#endif
