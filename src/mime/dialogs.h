/* $Id: dialogs.h,v 1.1 2002/08/08 18:01:46 pasky Exp $ */

#ifndef EL__DIALOGS_TYPES_H
#define EL__DIALOGS_TYPES_H

#include "lowlevel/terminal.h"

void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

#endif
