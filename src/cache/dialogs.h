/* $Id: dialogs.h,v 1.4 2003/11/21 22:39:27 jonas Exp $ */

#ifndef EL__CACHE_DIALOGS_H
#define EL__CACHE_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

extern struct hierbox_browser cache_browser;
void menu_cache_manager(struct terminal *, void *, struct session *);

#endif
