/* $Id: hierbox.h,v 1.4 2003/07/09 23:03:09 jonas Exp $ */

#ifndef EL__DIALOGS_HIERBOX_H
#define EL__DIALOGS_HIERBOX_H

#include "bfu/dialog.h"
#include "sched/session.h"
#include "terminal/terminal.h"

int hierbox_dialog_event_handler(struct dialog_data *, struct event *);
void layout_hierbox_browser(struct dialog_data *);

#endif
