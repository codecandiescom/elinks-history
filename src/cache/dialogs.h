/* $Id: dialogs.h,v 1.1 2003/11/17 17:58:57 pasky Exp $ */

#ifndef EL__CACHE_DIALOGS_H
#define EL__CACHE_DIALOGS_H

#include "terminal/terminal.h"
#include "sched/session.h"

void menu_cache_manager(struct terminal *, void *, struct session *);

void update_all_cache_dialogs(void);

#endif
