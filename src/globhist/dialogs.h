/* $Id: dialogs.h,v 1.6 2004/01/07 03:18:20 jonas Exp $ */

#ifndef EL__GLOBHIST_DIALOGS_H
#define EL__GLOBHIST_DIALOGS_H

#include "bfu/hierbox.h"
#include "terminal/terminal.h"
#include "sched/session.h"

extern struct hierbox_browser globhist_browser;
void history_manager(struct session *);

#endif
