/* $Id: hierbox.h,v 1.7 2003/11/08 22:23:33 jonas Exp $ */

#ifndef EL__DIALOGS_HIERBOX_H
#define EL__DIALOGS_HIERBOX_H

#include "bfu/dialog.h"
#include "sched/session.h"
#include "terminal/terminal.h"

int hierbox_dialog_event_handler(struct dialog_data *, struct term_event *);
void hierbox_dialog_abort_handler(struct dialog_data *dlg_data);
void hierbox_browser_layouter(struct dialog_data *);

#endif
