/* $Id: dialogs.h,v 1.11 2004/01/01 14:24:08 jonas Exp $ */

#ifndef EL__BOOKMARKS_DIALOGS_H
#define EL__BOOKMARKS_DIALOGS_H

#ifdef CONFIG_BOOKMARKS

#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "terminal/terminal.h"
#include "sched/session.h"

/* Search memorization */
extern unsigned char *bm_last_searched_name;
extern unsigned char *bm_last_searched_url;
extern struct hierbox_browser bookmark_browser;

/* Launch the bookmark manager */
void menu_bookmark_manager(struct terminal *term, void *fcp,
			   struct session *ses);

/* Launch 'Add bookmark' dialog... */

/* ...with the given title and URL */
void launch_bm_add_dialog(struct terminal *term,
			  struct dialog_data *parent,
			  struct session *ses,
			  unsigned char *title,
			  unsigned char *url);

/* ...with the current document's title and URL */
void launch_bm_add_doc_dialog(struct terminal *term,
			      struct dialog_data *parent,
			      struct session *ses);

/* ...with the selected link's title and URL */
void launch_bm_add_link_dialog(struct terminal *term,
			       struct dialog_data *parent,
			       struct session *ses);

void menu_bookmark_terminal_tabs(struct terminal *term, void *d, struct session *ses);

#endif
#endif
