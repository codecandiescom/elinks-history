/* $Id: menu.h,v 1.36 2004/05/29 15:36:39 jonas Exp $ */

#ifndef EL__DIALOG_MENU_H
#define EL__DIALOG_MENU_H

#include "sched/session.h"
#include "terminal/terminal.h"

struct open_in_new;
struct menu_item;
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

void open_url_in_new_window(struct session *ses, unsigned char *url, enum term_env_type);

void send_open_new_window(struct terminal *term, const struct open_in_new *open, struct session *ses);
void send_open_in_new_window(struct terminal *term, const struct open_in_new *open, struct session *ses);

void
open_in_new_window(struct terminal *term,
		   void (*)(struct terminal *, const struct open_in_new *, struct session *ses),
		   struct session *ses);

void
add_new_win_to_menu(struct menu_item **mi, unsigned char *text, int action,
		    struct terminal *term);

void add_uri_command_to_menu(struct menu_item **mi);
void pass_uri_to_command(struct session *ses, struct document_view *doc_view, int a);

#endif
