/* $Id: hierbox.h,v 1.9 2003/11/09 00:19:41 jonas Exp $ */

#ifndef EL__BFU_HIERBOX_H
#define EL__BFU_HIERBOX_H

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

int hierbox_dialog_event_handler(struct dialog_data *, struct term_event *);
void hierbox_dialog_abort_handler(struct dialog_data *dlg_data);
void hierbox_browser_layouter(struct dialog_data *);

struct dialog_data *
hierbox_browser(struct terminal *term, unsigned char *title, size_t add_size,
		struct listbox_data *listbox_data, void *udata,
		size_t buttons, ...);

#endif
