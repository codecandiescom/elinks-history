/* $Id: view.h,v 1.15 2003/07/03 01:23:00 pasky Exp $ */

#ifndef EL__VIEWER_TEXT_VIEW_H
#define EL__VIEWER_TEXT_VIEW_H

#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "terminal/terminal.h"
#include "sched/session.h"


void open_in_new_window(struct terminal *, void (*)(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *), struct session *);
/* void send_open_in_new_xterm(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *); */
void send_open_new_xterm(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *);

void destroy_formatted(struct f_data *);
/* void clear_formatted(struct f_data *); */
void init_formatted(struct f_data *);
void detach_formatted(struct f_data_c *);

/* void draw_doc(struct terminal *, struct f_data_c *, int); */
void draw_formatted(struct session *);

void send_event(struct session *, struct event *);

void save_as(struct terminal *, void *, struct session *);
void menu_save_formatted(struct terminal *, void *, struct session *);

void save_url(struct session *, unsigned char *);

void toggle(struct session *, struct f_data_c *, int);

void do_for_frame(struct session *, void (*)(struct session *, struct f_data_c *, int), int);

void set_frame(struct session *, struct f_data_c *, int);
struct f_data_c *current_frame(struct session *);

#endif
