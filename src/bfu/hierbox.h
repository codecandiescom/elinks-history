/* $Id: hierbox.h,v 1.6 2003/11/06 20:11:20 jonas Exp $ */

#ifndef EL__DIALOGS_HIERBOX_H
#define EL__DIALOGS_HIERBOX_H

#include "bfu/dialog.h"
#include "sched/session.h"
#include "terminal/terminal.h"

int hierbox_dialog_event_handler(struct dialog_data *, struct term_event *);
void hierbox_browser_layouter(struct dialog_data *);

#endif
