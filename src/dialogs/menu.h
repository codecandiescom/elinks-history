/* $Id: menu.h,v 1.28 2004/04/04 03:41:32 jonas Exp $ */

#ifndef EL__DIALOG_MENU_H
#define EL__DIALOG_MENU_H

#include "sched/session.h"
#include "terminal/terminal.h"

struct uri;

void activate_bfu_technology(struct session *, int);

void dialog_goto_url(struct session *ses, char *url);
/* void dialog_save_url(struct session *ses); */

void tab_menu(struct terminal *term, void *d, struct session *ses);

void free_history_lists(void);

void query_file(struct session *, struct uri *, void *, void (*)(void *, unsigned char *), void (*)(void *), int);

void really_exit_prog(struct session *ses);
void query_exit(struct session *ses);
void exit_prog(struct session *ses, int query);

void save_url_as(struct session *ses);

#endif
