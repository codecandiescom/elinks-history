/* $Id: view.h,v 1.19 2003/07/15 20:18:11 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_VIEW_H
#define EL__VIEWER_TEXT_VIEW_H

#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "sched/session.h"
#include "terminal/terminal.h"


void open_in_new_window(struct terminal *, void (*)(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *), struct session *);
/* void send_open_in_new_xterm(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *); */
void send_open_new_xterm(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *);

void destroy_formatted(struct document *);
/* void clear_formatted(struct document *); */
void init_formatted(struct document *);
void detach_formatted(struct document_view *);

/* void draw_doc(struct terminal *, struct document_view *, int); */
void draw_formatted(struct session *);

void send_event(struct session *, struct event *);

void save_as(struct terminal *, void *, struct session *);
void menu_save_formatted(struct terminal *, void *, struct session *);

void save_url(struct session *, unsigned char *);

void toggle(struct session *, struct document_view *, int);

void do_for_frame(struct session *, void (*)(struct session *, struct document_view *, int), int);

void set_frame(struct session *, struct document_view *, int);
struct document_view *current_frame(struct session *);

/* Bruteforce compilation fixes */
void down(struct session *ses, struct document_view *fd, int a);
inline void decrement_fc_refcount(struct document *f);
void draw_doc(struct terminal *t, struct document_view *scr, int active);
void send_enter(struct terminal *term, void *xxx, struct session *ses);
void send_enter_reload(struct terminal *term, void *xxx, struct session *ses);
void send_download(struct terminal *term, void *xxx, struct session *ses);
void send_open_in_new_xterm(struct terminal *term,
	void (*open_window)(struct terminal *, unsigned char *, unsigned char *),
	struct session *ses);
void send_image(struct terminal *term, void *xxx, struct session *ses);
void send_download_image(struct terminal *term, void *xxx, struct session *ses);

#endif
