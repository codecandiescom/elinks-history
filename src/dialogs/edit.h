/* $Id: edit.h,v 1.1 2002/04/02 14:11:57 pasky Exp $ */

#ifndef EL__DIALOGS_EDIT_H
#define EL__DIALOGS_EDIT_H

#include <bfu/bfu.h>
#include <document/session.h>
#include <lowlevel/terminal.h>

void do_edit_dialog(struct terminal *, unsigned char *,
		    const unsigned char *, const unsigned char *,
		    struct session *, struct dialog_data *,
		    void when_done(struct dialog *),
		    void *, int);

#endif
