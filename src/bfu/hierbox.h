/* $Id: hierbox.h,v 1.5 2003/09/25 19:22:51 zas Exp $ */

#ifndef EL__DIALOGS_HIERBOX_H
#define EL__DIALOGS_HIERBOX_H

#include "bfu/dialog.h"
#include "sched/session.h"
#include "terminal/terminal.h"

int hierbox_dialog_event_handler(struct dialog_data *, struct term_event *);
void layout_hierbox_browser(struct dialog_data *);

#endif
