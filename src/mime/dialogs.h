/* $Id: dialogs.h,v 1.3 2003/12/31 12:53:40 pasky Exp $ */

#ifndef EL__MIME_DIALOGS_H
#define EL__MIME_DIALOGS_H

#include "terminal/terminal.h"

void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

#endif
