/* $Id: dialogs.h,v 1.5 2003/11/21 22:39:27 jonas Exp $ */

#ifndef EL__GLOBHIST_DIALOGS_H
#define EL__GLOBHIST_DIALOGS_H

#include "bfu/hierbox.h"
#include "terminal/terminal.h"
#include "sched/session.h"

extern struct hierbox_browser globhist_browser;
void menu_history_manager(struct terminal *, void *, struct session *);

#endif
