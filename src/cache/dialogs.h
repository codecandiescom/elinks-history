/* $Id: dialogs.h,v 1.2 2003/11/17 18:03:29 jonas Exp $ */

#ifndef EL__CACHE_DIALOGS_H
#define EL__CACHE_DIALOGS_H

#include "sched/session.h"
#include "terminal/terminal.h"

void menu_cache_manager(struct terminal *, void *, struct session *);

void update_all_cache_dialogs(void);

#endif
