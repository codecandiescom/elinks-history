/* $Id: dialogs.h,v 1.5 2003/01/05 16:48:13 pasky Exp $ */

#ifndef EL__BOOKMARKS_DIALOGS_H
#define EL__BOOKMARKS_DIALOGS_H

#include "bfu/dialog.h"
#include "lowlevel/terminal.h"
#include "sched/session.h"

/* Search memorization */
extern unsigned char *bm_last_searched_name;
extern unsigned char *bm_last_searched_url;

/* Launches bookmark manager */
void menu_bookmark_manager(struct terminal *, void *, struct session *);

/* Launches add dialogs */
void launch_bm_add_doc_dialog(struct terminal *, struct dialog_data *, struct session *);
void launch_bm_add_link_dialog(struct terminal *, struct dialog_data *, struct session *);

#endif
