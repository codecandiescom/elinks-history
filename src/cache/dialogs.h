/* $Id: dialogs.h,v 1.3 2003/11/19 01:45:05 jonas Exp $ */

#ifndef EL__CACHE_DIALOGS_H
#define EL__CACHE_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

struct hierbox_browser cache_browser;
void menu_cache_manager(struct terminal *, void *, struct session *);

#endif
