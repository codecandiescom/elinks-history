/* $Id: menu.h,v 1.4 2002/07/04 12:07:26 pasky Exp $ */

#ifndef EL__DIALOG_MENU_H
#define EL__DIALOG_MENU_H

#include "document/session.h"
#include "document/html/renderer.h"
#include "lowlevel/terminal.h"

void activate_bfu_technology(struct session *, int);

void dialog_goto_url(struct session *ses, char *url);
/* void dialog_save_url(struct session *ses); */

void free_history_lists();

void query_file(struct session *, unsigned char *, void (*)(struct session *, unsigned char *), void (*)(struct session *));

void search_dlg(struct session *, struct f_data_c *, int);
void search_back_dlg(struct session *, struct f_data_c *, int);

void exit_prog(struct terminal *, void *, struct session *);

void do_auth_dialog(struct session *);

#endif
