/* $Id: edit.h,v 1.3 2002/07/04 21:19:45 pasky Exp $ */

#ifndef EL__DIALOGS_EDIT_H
#define EL__DIALOGS_EDIT_H

#include "bfu/dialog.h"
#include "document/session.h"
#include "lowlevel/terminal.h"

void do_edit_dialog(struct terminal *, unsigned char *,
		    const unsigned char *, const unsigned char *,
		    struct session *, struct dialog_data *,
		    void when_done(struct dialog *),
		    void *, int);

#endif
