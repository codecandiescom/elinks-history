/* $Id: link.h,v 1.3 2003/07/03 02:18:54 jonas Exp $ */

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

struct link *get_first_link(struct f_data_c *f);
struct link *get_last_link(struct f_data_c *f);
struct link *choose_mouse_link(struct f_data_c *f, struct event *ev);

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
void goto_link_number(struct session *ses, unsigned char *num);

/* Bruteforce compilation fixes */
int enter(struct session *ses, struct f_data_c *fd, int a);
int try_document_key(struct session *ses, struct f_data_c *fd, struct event *ev);
int in_viewy(struct f_data_c *f, struct link *l);
unsigned char *get_link_url(struct session *ses, struct f_data_c *f, struct link *l);

#endif
