/* $Id: info.h,v 1.4 2003/05/04 17:25:53 pasky Exp $ */

#ifndef EL__DIALOG_INFO_H
#define EL__DIALOG_INFO_H

#include "terminal/terminal.h"
#include "sched/session.h"

void menu_about(struct terminal *term, void *d, struct session *ses);
void menu_keys(struct terminal *term, void *d, struct session *ses);
void menu_copying(struct terminal *term, void *d, struct session *ses);

void res_inf(struct terminal *term, void *d, struct session *ses);
void cache_inf(struct terminal *term, void *d, struct session *ses);
void memory_inf(struct terminal *term, void *d, struct session *ses);

#endif
