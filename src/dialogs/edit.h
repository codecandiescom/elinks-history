/* $Id: edit.h,v 1.8 2003/07/09 23:03:09 jonas Exp $ */

#ifndef EL__DIALOGS_EDIT_H
#define EL__DIALOGS_EDIT_H

#include "bfu/dialog.h"
#include "sched/session.h"
#include "terminal/terminal.h"

void do_edit_dialog(struct terminal *, int, unsigned char *,
		    const unsigned char *, const unsigned char *,
		    struct session *, struct dialog_data *,
		    void when_done(struct dialog *),
		    void when_cancel(struct dialog *),
		    void *, int);

#endif
