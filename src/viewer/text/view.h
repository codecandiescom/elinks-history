/* $Id: view.h,v 1.30 2003/10/23 23:09:42 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_VIEW_H
#define EL__VIEWER_TEXT_VIEW_H

#include "document/html/frames.h"
#include "sched/session.h"
#include "terminal/event.h"
#include "terminal/terminal.h"


/* Initializes a document and it's canvas. The @uristring should match a
 * cache_entry. */
/* Return NULL on allocation failure. */
struct document *
init_document(unsigned char *uristring, struct document_options *options);

/* Releases the document and all it's resources. */
void done_document(struct document *document);

/* Releases the document view's resources. But doesn't free() the @view. */
void detach_formatted(struct document_view *doc_view);

/* Puts the formatted document on the given terminal's screen. */
void draw_doc(struct terminal *t, struct document_view *doc_view, int active);

void draw_formatted(struct session *);

void set_frame(struct session *, struct document_view *doc_view, int);
struct document_view *current_frame(struct session *);

void down(struct session *ses, struct document_view *doc_view, int a);

/* Used for changing between formatted and source (plain) view. */
void toggle_plain_html(struct session *ses, struct document_view *doc_view, int a);

/* Toggle images rendering */
void toggle_images(struct session *ses, struct document_view *doc_view, int a);

/* Toggle link numbering */
void toggle_link_numbering(struct session *ses, struct document_view *doc_view, int a);

/* Toggle document colors */
void toggle_document_colors(struct session *ses, struct document_view *doc_view, int a);

/* File menu handlers. */

void save_as(struct terminal *, void *, struct session *);
void save_url(struct session *, unsigned char *);

/* Various event emitters and link menu handlers. */

void send_event(struct session *, struct term_event *);
void send_enter(struct terminal *term, void *xxx, struct session *ses);
void send_enter_reload(struct terminal *term, void *xxx, struct session *ses);

void send_image(struct terminal *term, void *xxx, struct session *ses);
void send_download(struct terminal *term, void *xxx, struct session *ses);
void send_download_image(struct terminal *term, void *xxx, struct session *ses);

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

void menu_save_formatted(struct terminal *, void *, struct session *);

#endif
