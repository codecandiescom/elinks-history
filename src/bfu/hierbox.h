/* $Id: hierbox.h,v 1.1 2002/09/17 17:17:14 pasky Exp $ */

#ifndef EL__DIALOGS_HIERBOX_H
#define EL__DIALOGS_HIERBOX_H

#include "bfu/dialog.h"
#include "document/session.h"
#include "lowlevel/terminal.h"

int hierbox_dialog_event_handler(struct dialog_data *, struct event *);
void layout_hierbox_browser(struct dialog_data *);

#endif
