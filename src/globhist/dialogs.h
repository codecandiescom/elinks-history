/* $Id: dialogs.h,v 1.2 2003/01/05 16:48:15 pasky Exp $ */

#ifndef EL__GLOBHIST_DIALOGS_H
#define EL__GLOBHIST_DIALOGS_H

#include "lowlevel/terminal.h"
#include "sched/session.h"

void menu_history_manager(struct terminal *, void *, struct session *);

void update_all_history_dialogs(void);

#endif
