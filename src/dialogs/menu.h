/* $Id: menu.h,v 1.26 2004/01/08 13:28:53 jonas Exp $ */

#ifndef EL__DIALOG_MENU_H
#define EL__DIALOG_MENU_H

#include "sched/session.h"
#include "terminal/terminal.h"

void activate_bfu_technology(struct session *, int);

void dialog_goto_url(struct session *ses, char *url);
/* void dialog_save_url(struct session *ses); */

void tab_menu(struct terminal *term, void *d, struct session *ses);

void free_history_lists(void);

void query_file(struct session *, unsigned char *, void *, void (*)(void *, unsigned char *), void (*)(void *), int);

void do_auth_dialog(struct session *);

void really_exit_prog(struct session *ses);
void query_exit(struct session *ses);
void exit_prog(struct session *ses, int query);

void save_url_as(struct session *ses);

#endif
