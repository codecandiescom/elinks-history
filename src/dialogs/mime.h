/* $Id: mime.h,v 1.2 2003/05/04 17:25:53 pasky Exp $ */

#ifndef EL__DIALOGS_TYPES_H
#define EL__DIALOGS_TYPES_H

#include "terminal/terminal.h"

void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

#endif
