/* $Id: dialogs.h,v 1.4 2004/01/07 03:18:19 jonas Exp $ */

#ifndef EL__COOKIES_DIALOGS_H
#define EL__COOKIES_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

extern struct hierbox_browser cookie_browser;
void cookie_manager(struct session *);

#endif
