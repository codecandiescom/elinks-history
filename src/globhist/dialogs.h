/* $Id: dialogs.h,v 1.3 2003/05/04 17:25:53 pasky Exp $ */

#ifndef EL__GLOBHIST_DIALOGS_H
#define EL__GLOBHIST_DIALOGS_H

#include "terminal/terminal.h"
#include "sched/session.h"

void menu_history_manager(struct terminal *, void *, struct session *);

void update_all_history_dialogs(void);

#endif
