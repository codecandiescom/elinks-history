/* $Id: info.h,v 1.8 2004/07/28 02:20:01 jonas Exp $ */

#ifndef EL__DIALOGS_INFO_H
#define EL__DIALOGS_INFO_H

struct session;
struct terminal;

void menu_about(struct terminal *, void *, struct session *);
void menu_keys(struct terminal *, void *, struct session *);
void menu_copying(struct terminal *, void *, struct session *);

void resource_info(struct terminal *term);
void memory_inf(struct terminal *, void *, struct session *);

#endif
