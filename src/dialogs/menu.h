/* $Id: menu.h,v 1.1 2002/03/17 14:39:12 pasky Exp $ */

#ifndef EL__MENU_H
#define EL__MENU_H

#include <document/session.h>
#include <document/html/renderer.h>
#include <lowlevel/terminal.h>

void activate_bfu_technology(struct session *, int);

void dialog_goto_url(struct session *ses, char *url);
/* void dialog_save_url(struct session *ses); */
void dialog_lua_console(struct session *ses);

void free_history_lists();

void query_file(struct session *, unsigned char *, void (*)(struct session *, unsigned char *), void (*)(struct session *));

void search_dlg(struct session *, struct f_data_c *, int);
void search_back_dlg(struct session *, struct f_data_c *, int);

void exit_prog(struct terminal *, void *, struct session *);

void do_auth_dialog(struct session *);

#endif
