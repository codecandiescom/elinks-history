/* $Id: info.h,v 1.6 2003/07/09 23:03:09 jonas Exp $ */

#ifndef EL__DIALOG_INFO_H
#define EL__DIALOG_INFO_H

#include "sched/session.h"
#include "terminal/terminal.h"

void menu_about(struct terminal *, void *, struct session *);
void menu_keys(struct terminal *, void *, struct session *);
void menu_copying(struct terminal *, void *, struct session *);

void res_inf(struct terminal *, void *, struct session *);
void cache_inf(struct terminal *, void *, struct session *);
void memory_inf(struct terminal *, void *, struct session *);

#endif
