/* $Id: link.h,v 1.24 2004/05/25 00:48:40 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_LINK_H
#define EL__VIEWER_TEXT_LINK_H

struct document;
struct document_view;
struct link;
struct session;
struct term_event;
struct terminal;
struct uri;

void set_link(struct document_view *doc_view);
void free_link(struct document_view *doc_view);
void clear_link(struct terminal *t, struct document_view *doc_view);
void draw_current_link(struct terminal *t, struct document_view *doc_view);

void link_menu(struct terminal *, void *, struct session *);
void selected_item(struct terminal *, void *, struct session *);

int get_current_state(struct session *);

struct link *get_first_link(struct document_view *doc_view);
struct link *get_last_link(struct document_view *doc_view);
struct link *choose_mouse_link(struct document_view *doc_view, struct term_event *ev);

unsigned char *print_current_link_title_do(struct document_view *doc_view, struct terminal *);
unsigned char *print_current_link_do(struct document_view *doc_view, struct terminal *);
unsigned char *print_current_link(struct session *);

void set_pos_x(struct document_view *doc_view, struct link *);
void set_pos_y(struct document_view *doc_view, struct link *);
void find_link(struct document_view *doc_view, int, int);
int current_link_is_visible(struct document_view *doc_view);
int in_view(struct document_view *doc_view, struct link *l);
int next_in_view(struct document_view *doc_view, int p, int d, int (*fn)(struct document_view *, struct link *), void (*cntr)(struct document_view *, struct link *));

void jump_to_link_number(struct session *, struct document_view *doc_view, int);

struct link *goto_current_link(struct session *, struct document_view *, int);
void goto_link_number(struct session *ses, unsigned char *num);

/* Bruteforce compilation fixes */
int enter(struct session *ses, struct document_view *doc_view, int a);
int try_document_key(struct session *ses, struct document_view *doc_view, struct term_event *ev);
int in_viewy(struct document_view *doc_view, struct link *l);

struct uri *get_link_uri(struct session *ses, struct document_view *doc_view, struct link *link);

#endif
