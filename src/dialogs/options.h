/* $Id: options.h,v 1.1 2002/04/02 21:50:37 pasky Exp $ */

#ifndef EL__DIALOGS_OPTIONS_H
#define EL__DIALOGS_OPTIONS_H

#include <document/session.h>
#include <lowlevel/terminal.h>

void charset_list(struct terminal *, void *, struct session *);
void terminal_options(struct terminal *, void *, struct session *);
void net_options(struct terminal *, void *, void *);
void net_programs(struct terminal *, void *, void *);
void cache_opt(struct terminal *, void *, void *);
void menu_save_html_options(struct terminal *, void *, struct session *);
void menu_html_options(struct terminal *, void *, struct session *);
void menu_language_list(struct terminal *, void *, struct session *);
void dlg_resize_terminal(struct terminal *, void *, struct session *);

#endif
