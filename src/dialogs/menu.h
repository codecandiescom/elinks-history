/* $Id: menu.h,v 1.10 2003/07/09 23:03:09 jonas Exp $ */

#ifndef EL__DIALOG_MENU_H
#define EL__DIALOG_MENU_H

#include "document/html/renderer.h"
#include "sched/session.h"
#include "terminal/terminal.h"

void activate_bfu_technology(struct session *, int);

void dialog_goto_url(struct session *ses, char *url);
/* void dialog_save_url(struct session *ses); */

void free_history_lists(void);

void query_file(struct session *, unsigned char *, void (*)(struct session *, unsigned char *), void (*)(struct session *), int);

void search_dlg(struct session *, struct f_data_c *, int);
void search_back_dlg(struct session *, struct f_data_c *, int);

void exit_prog(struct terminal *, void *, struct session *);

void do_auth_dialog(struct session *);

void really_exit_prog(struct session *ses);

#endif
