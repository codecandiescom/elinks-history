/* $Id: link.h,v 1.1 2003/07/03 01:23:00 pasky Exp $ */

#ifndef EL__VIEWER_TEXT_LINK_H
#define EL__VIEWER_TEXT_LINK_H

#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "terminal/terminal.h"
#include "sched/session.h"

void sort_links(struct f_data *);

void set_link(struct f_data_c *f);
void free_link(struct f_data_c *scr);
void clear_link(struct terminal *t, struct f_data_c *scr);
void draw_current_link(struct terminal *t, struct f_data_c *scr);

void link_menu(struct terminal *, void *, struct session *);
void selected_item(struct terminal *, void *, struct session *);

int get_current_state(struct session *);

unsigned char *print_current_link_title_do(struct f_data_c *, struct terminal *);
unsigned char *print_current_link_do(struct f_data_c *, struct terminal *);
unsigned char *print_current_link(struct session *);
unsigned char *print_current_title(struct session *);

void set_pos_x(struct f_data_c *, struct link *);
void set_pos_y(struct f_data_c *, struct link *);
void find_link(struct f_data_c *, int, int);
int c_in_view(struct f_data_c *);
int in_view(struct f_data_c *f, struct link *l);
int next_in_view(struct f_data_c *f, int p, int d, int (*fn)(struct f_data_c *, struct link *), void (*cntr)(struct f_data_c *, struct link *));

void jump_to_link_number(struct session *, struct f_data_c *, int);

int goto_link(unsigned char *, unsigned char *, struct session *, int);

#endif
