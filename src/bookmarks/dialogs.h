/* $Id: dialogs.h,v 1.3 2002/07/04 21:19:45 pasky Exp $ */

#ifndef EL__BOOKMARKS_DIALOGS_H
#define EL__BOOKMARKS_DIALOGS_H

#include "bfu/dialog.h"
#include "document/session.h"
#include "lowlevel/terminal.h"

/* Launches bookmark manager */
void menu_bookmark_manager(struct terminal *, void *, struct session *);

/* Launches add dialogs */
void launch_bm_add_doc_dialog(struct terminal *, struct dialog_data *, struct session *);
void launch_bm_add_link_dialog(struct terminal *, struct dialog_data *, struct session *);

#endif
