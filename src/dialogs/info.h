/* $Id: info.h,v 1.2 2002/05/08 13:55:02 pasky Exp $ */

#ifndef EL__DIALOG_INFO_H
#define EL__DIALOG_INFO_H

#include "document/session.h"
#include "lowlevel/terminal.h"

void menu_about(struct terminal *term, void *d, struct session *ses);
void menu_keys(struct terminal *term, void *d, struct session *ses);
void menu_copying(struct terminal *term, void *d, struct session *ses);

void res_inf(struct terminal *term, void *d, struct session *ses);
void cache_inf(struct terminal *term, void *d, struct session *ses);
void memory_inf(struct terminal *term, void *d, struct session *ses);

#endif
