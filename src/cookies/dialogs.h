/* $Id: dialogs.h,v 1.6 2005/02/04 21:49:26 pasky Exp $ */

#ifndef EL__COOKIES_DIALOGS_H
#define EL__COOKIES_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/lists.h"

extern struct list_head cookie_queries;

void accept_cookie_dialog(struct session *ses, void *data);
extern struct hierbox_browser cookie_browser;
void cookie_manager(struct session *);

#endif
