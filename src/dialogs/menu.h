/* $Id: menu.h,v 1.12 2003/10/04 22:21:34 pasky Exp $ */

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

void exit_prog(struct terminal *, void *, struct session *);

void do_auth_dialog(struct session *);

void really_exit_prog(struct session *ses);

#endif
