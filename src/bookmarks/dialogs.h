/* $Id: dialogs.h,v 1.4 2002/12/05 21:30:05 pasky Exp $ */

#ifndef EL__BOOKMARKS_DIALOGS_H
#define EL__BOOKMARKS_DIALOGS_H

#include "bfu/dialog.h"
#include "document/session.h"
#include "lowlevel/terminal.h"

/* Search memorization */
extern unsigned char *bm_last_searched_name;
extern unsigned char *bm_last_searched_url;

/* Launches bookmark manager */
void menu_bookmark_manager(struct terminal *, void *, struct session *);

/* Launches add dialogs */
void launch_bm_add_doc_dialog(struct terminal *, struct dialog_data *, struct session *);
void launch_bm_add_link_dialog(struct terminal *, struct dialog_data *, struct session *);

#endif
