/* $Id: bookmarks.h,v 1.3 2002/03/30 21:17:46 pasky Exp $ */

#ifndef EL__BOOKMARKS_H
#define EL__BOOKMARKS_H

#include <bfu/bfu.h>
#include <document/session.h>
#include <lowlevel/terminal.h>

/* Read/write bookmarks functions */
void read_bookmarks();
/* void write_bookmarks(); */

/* Cleanups and saves bookmarks */
void finalize_bookmarks();

/* Launches bookmark manager */
void menu_bookmark_manager(struct terminal *, void *, struct session *);

/* Launches add dialogs */
void launch_bm_add_doc_dialog(struct terminal *, struct dialog_data *, struct session *);
void launch_bm_add_link_dialog(struct terminal *, struct dialog_data *, struct session *);

#endif
