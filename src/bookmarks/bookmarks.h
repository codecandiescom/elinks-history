/* $Id: bookmarks.h,v 1.1 2002/03/17 11:29:10 pasky Exp $ */

#ifndef EL__BOOKMARKS_H
#define EL__BOOKMARKS_H

#include "bfu.h"
#include "session.h"
#include "terminal.h"

/* Read/write bookmarks functions */
void read_bookmarks();
/* void write_bookmarks(); */

/* Cleanups and saves bookmarks */
void finalize_bookmarks();

/* Launches bookmark manager */
void menu_bookmark_manager(struct terminal *, void *, struct session *);

/* Launches add dialogs */
void launch_bm_add_doc_dialog(struct terminal *, struct dialog_data *, struct session *);
/* void launch_bm_add_link_dialog(struct terminal *, struct dialog_data *, struct session *); */

#endif
