/* $Id: globhist.h,v 1.3 2002/05/08 13:55:02 pasky Exp $ */

#ifndef EL__DIALOGS_GLOBHIST_H
#define EL__DIALOGS_GLOBHIST_H

#include "document/session.h"
#include "lowlevel/terminal.h"

void menu_history_manager(struct terminal *, void *, struct session *);

void update_all_history_dialogs(void);

#endif
