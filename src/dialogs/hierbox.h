/* $Id: hierbox.h,v 1.2 2003/01/05 16:48:13 pasky Exp $ */

#ifndef EL__DIALOGS_HIERBOX_H
#define EL__DIALOGS_HIERBOX_H

#include "bfu/dialog.h"
#include "lowlevel/terminal.h"
#include "sched/session.h"

int hierbox_dialog_event_handler(struct dialog_data *, struct event *);
void layout_hierbox_browser(struct dialog_data *);

#endif
