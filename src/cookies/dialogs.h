/* $Id: dialogs.h,v 1.2 2003/11/19 01:45:06 jonas Exp $ */

#ifndef EL__COOKIES_DIALOGS_H
#define EL__COOKIES_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

struct hierbox_browser cookie_browser;
void menu_cookie_manager(struct terminal *, void *, struct session *);

#endif
