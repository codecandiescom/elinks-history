/* $Id: hierbox.h,v 1.10 2003/11/09 03:30:46 jonas Exp $ */

#ifndef EL__BFU_HIERBOX_H
#define EL__BFU_HIERBOX_H

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

struct dialog_data *
hierbox_browser(struct terminal *term, unsigned char *title, size_t add_size,
		struct listbox_data *listbox_data, void *udata,
		size_t buttons, ...);

#endif
