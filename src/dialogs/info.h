/* $Id: info.h,v 1.5 2003/05/07 13:38:44 zas Exp $ */

#ifndef EL__DIALOG_INFO_H
#define EL__DIALOG_INFO_H

#include "terminal/terminal.h"
#include "sched/session.h"

void menu_about(struct terminal *, void *, struct session *);
void menu_keys(struct terminal *, void *, struct session *);
void menu_copying(struct terminal *, void *, struct session *);

void res_inf(struct terminal *, void *, struct session *);
void cache_inf(struct terminal *, void *, struct session *);
void memory_inf(struct terminal *, void *, struct session *);

#endif
