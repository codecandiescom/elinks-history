/* $Id: link.h,v 1.6 2003/07/15 20:18:10 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_LINK_H
#define EL__VIEWER_TEXT_LINK_H

#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "sched/session.h"
#include "terminal/terminal.h"

void sort_links(struct document *);

void set_link(struct document_view *f);
void free_link(struct document_view *scr);
void clear_link(struct terminal *t, struct document_view *scr);
void draw_current_link(struct terminal *t, struct document_view *scr);

void link_menu(struct terminal *, void *, struct session *);
void selected_item(struct terminal *, void *, struct session *);

int get_current_state(struct session *);

struct link *get_first_link(struct document_view *f);
struct link *get_last_link(struct document_view *f);
struct link *choose_mouse_link(struct document_view *f, struct event *ev);

unsigned char *print_current_link_title_do(struct document_view *, struct terminal *);
unsigned char *print_current_link_do(struct document_view *, struct terminal *);
unsigned char *print_current_link(struct session *);
unsigned char *print_current_title(struct session *);

void set_pos_x(struct document_view *, struct link *);
void set_pos_y(struct document_view *, struct link *);
void find_link(struct document_view *, int, int);
int c_in_view(struct document_view *);
int in_view(struct document_view *f, struct link *l);
int next_in_view(struct document_view *f, int p, int d, int (*fn)(struct document_view *, struct link *), void (*cntr)(struct document_view *, struct link *));

void jump_to_link_number(struct session *, struct document_view *, int);

int goto_link(unsigned char *, unsigned char *, struct session *, int);
void goto_link_number(struct session *ses, unsigned char *num);

/* Bruteforce compilation fixes */
int enter(struct session *ses, struct document_view *fd, int a);
int try_document_key(struct session *ses, struct document_view *fd, struct event *ev);
int in_viewy(struct document_view *f, struct link *l);
unsigned char *get_link_url(struct session *ses, struct document_view *f, struct link *l);

#endif
