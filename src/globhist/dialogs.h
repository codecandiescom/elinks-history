/* $Id: dialogs.h,v 1.1 2002/08/29 23:58:23 pasky Exp $ */

#ifndef EL__GLOBHIST_DIALOGS_H
#define EL__GLOBHIST_DIALOGS_H

#include "document/session.h"
#include "lowlevel/terminal.h"

void menu_history_manager(struct terminal *, void *, struct session *);

void update_all_history_dialogs(void);

#endif
