/* $Id: dialogs.h,v 1.1 2003/11/17 22:11:38 jonas Exp $ */

#ifndef EL__COOKIES_DIALOGS_H
#define EL__COOKIES_DIALOGS_H

#include "sched/session.h"
#include "terminal/terminal.h"

void menu_cookie_manager(struct terminal *, void *, struct session *);

void update_all_cookie_dialogs(void);

#endif
