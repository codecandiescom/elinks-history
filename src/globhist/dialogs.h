/* $Id: dialogs.h,v 1.4 2003/11/19 01:45:06 jonas Exp $ */

#ifndef EL__GLOBHIST_DIALOGS_H
#define EL__GLOBHIST_DIALOGS_H

#include "bfu/hierbox.h"
#include "terminal/terminal.h"
#include "sched/session.h"

struct hierbox_browser globhist_browser;
void menu_history_manager(struct terminal *, void *, struct session *);

#endif
