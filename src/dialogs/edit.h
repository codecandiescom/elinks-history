/* $Id: edit.h,v 1.6 2003/05/04 17:25:53 pasky Exp $ */

#ifndef EL__DIALOGS_EDIT_H
#define EL__DIALOGS_EDIT_H

#include "bfu/dialog.h"
#include "terminal/terminal.h"
#include "sched/session.h"

void do_edit_dialog(struct terminal *, unsigned char *,
		    const unsigned char *, const unsigned char *,
		    struct session *, struct dialog_data *,
		    void when_done(struct dialog *),
		    void when_cancel(struct dialog *),
		    void *, int);

#endif
