/* $Id: dialogs.h,v 1.5 2004/11/19 15:53:54 jonas Exp $ */

#ifndef EL__COOKIES_DIALOGS_H
#define EL__COOKIES_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/lists.h"

struct list_head cookie_queries;

void accept_cookie_dialog(struct session *ses, void *data);
extern struct hierbox_browser cookie_browser;
void cookie_manager(struct session *);

#endif
