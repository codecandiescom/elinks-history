/* $Id: menu.h,v 1.30 2004/04/15 16:44:57 jonas Exp $ */

#ifndef EL__DIALOG_MENU_H
#define EL__DIALOG_MENU_H

#include "sched/session.h"
#include "terminal/terminal.h"

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

void
open_url_in_new_window(struct session *ses, unsigned char *url,
			void (*open_window)(struct terminal *, unsigned char *, unsigned char *));

void send_open_new_window(struct terminal *,
			 void (*)(struct terminal *, unsigned char *, unsigned char *),
			 struct session *);

void send_open_in_new_window(struct terminal *term,
			    void (*open_window)(struct terminal *, unsigned char *, unsigned char *),
			    struct session *ses);

void
open_in_new_window(struct terminal *term,
		   void (*)(struct terminal *,
			    void (*)(struct terminal *, unsigned char *, unsigned char *),
			    struct session *ses),
		   struct session *ses);

void
add_new_win_to_menu(struct menu_item **mi, unsigned char *text, int action,
		    struct terminal *term);

#endif
