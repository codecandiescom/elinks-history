/* $Id: dialogs.h,v 1.3 2003/11/21 22:39:27 jonas Exp $ */

#ifndef EL__COOKIES_DIALOGS_H
#define EL__COOKIES_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

extern struct hierbox_browser cookie_browser;
void menu_cookie_manager(struct terminal *, void *, struct session *);

#endif
