/* $Id: edit.h,v 1.4 2002/08/30 23:32:57 pasky Exp $ */

#ifndef EL__DIALOGS_EDIT_H
#define EL__DIALOGS_EDIT_H

#include "bfu/dialog.h"
#include "document/session.h"
#include "lowlevel/terminal.h"

void do_edit_dialog(struct terminal *, unsigned char *,
		    const unsigned char *, const unsigned char *,
		    struct session *, struct dialog_data *,
		    void when_done(struct dialog *),
		    void when_cancel(struct dialog *),
		    void *, int);

#endif
