/* $Id: info.h,v 1.7 2004/01/08 18:37:46 jonas Exp $ */

#ifndef EL__DIALOG_INFO_H
#define EL__DIALOG_INFO_H

#include "sched/session.h"
#include "terminal/terminal.h"

void menu_about(struct terminal *, void *, struct session *);
void menu_keys(struct terminal *, void *, struct session *);
void menu_copying(struct terminal *, void *, struct session *);

void resource_info(struct terminal *term);
void memory_inf(struct terminal *, void *, struct session *);

#endif
