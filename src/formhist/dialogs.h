/* $Id: dialogs.h,v 1.1 2003/11/24 16:23:39 jonas Exp $ */

#ifndef EL__FORMHIST_DIALOGS_H
#define EL__FORMHIST_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

extern struct hierbox_browser formhist_browser;
void menu_formhist_manager(struct terminal *, void *, struct session *);

#endif
