/* $Id: dialogs.h,v 1.2 2004/01/07 03:18:20 jonas Exp $ */

#ifndef EL__FORMHIST_DIALOGS_H
#define EL__FORMHIST_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

extern struct hierbox_browser formhist_browser;
void formhist_manager(struct session *);

#endif
