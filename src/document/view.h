/* $Id: view.h,v 1.9 2002/06/07 19:53:45 pasky Exp $ */

#ifndef EL__DOCUMENT_VIEW_H
#define EL__DOCUMENT_VIEW_H

#include "document/location.h"
#include "document/session.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "lowlevel/terminal.h"

extern int textarea_editor;
void textarea_edit(int, struct terminal *, struct form_control *, struct form_state *, struct f_data_c *, struct link *);

int can_open_in_new(struct terminal *);
void open_in_new_window(struct terminal *, void (*)(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *), struct session *);
/* void send_open_in_new_xterm(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *); */
void send_open_new_xterm(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *);

void sort_links(struct f_data *);

void destroy_formatted(struct f_data *);
/* void clear_formatted(struct f_data *); */
void init_formatted(struct f_data *);
void detach_formatted(struct f_data_c *);

/* void draw_doc(struct terminal *, struct f_data_c *, int); */
void draw_formatted(struct session *);

void send_event(struct session *, struct event *);

void link_menu(struct terminal *, void *, struct session *);
void save_as(struct terminal *, void *, struct session *);
void menu_save_formatted(struct terminal *, void *, struct session *);
void selected_item(struct terminal *, void *, struct session *);

void save_url(struct session *, unsigned char *);

void toggle(struct session *, struct f_data_c *, int);

void do_for_frame(struct session *, void (*)(struct session *, struct f_data_c *, int), int);

int get_current_state(struct session *);

unsigned char *print_current_link_title_do(struct f_data_c *, struct terminal *);
unsigned char *print_current_link_do(struct f_data_c *, struct terminal *);
unsigned char *print_current_link(struct session *);
unsigned char *print_current_title(struct session *);

void search_for(struct session *, unsigned char *);
void search_for_back(struct session *, unsigned char *);
void find_next(struct session *, struct f_data_c *, int);
void find_next_back(struct session *, struct f_data_c *, int);

void set_frame(struct session *, struct f_data_c *, int);
struct f_data_c *current_frame(struct session *);

void set_pos_x(struct f_data_c *, struct link *);
void set_pos_y(struct f_data_c *, struct link *);
void find_link(struct f_data_c *, int, int);
int c_in_view(struct f_data_c *);

#endif
