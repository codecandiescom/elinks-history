/* $Id: edit.h,v 1.9 2003/09/25 14:29:05 zas Exp $ */

#ifndef EL__DIALOGS_EDIT_H
#define EL__DIALOGS_EDIT_H

#include "bfu/dialog.h"
#include "sched/session.h"
#include "terminal/terminal.h"

enum edit_dialog_type {
	EDIT_DLG_SEARCH,	/* search dialog */
	EDIT_DLG_ADD		/* edit/add dialog */
};

void do_edit_dialog(struct terminal *, int, unsigned char *,
		    const unsigned char *, const unsigned char *,
		    struct session *, struct dialog_data *,
		    void when_done(struct dialog *),
		    void when_cancel(struct dialog *),
		    void *, enum edit_dialog_type);

#endif
