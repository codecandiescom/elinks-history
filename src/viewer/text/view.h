/* $Id: view.h,v 1.50 2004/06/09 21:42:19 zas Exp $ */

#ifndef EL__VIEWER_TEXT_VIEW_H
#define EL__VIEWER_TEXT_VIEW_H

struct document_view;
struct session;
struct term_event;
struct terminal;


/* Releases the document view's resources. But doesn't free() the @view. */
void detach_formatted(struct document_view *doc_view);

/* Puts the formatted document on the given terminal's screen. */
void draw_doc(struct session *ses, struct document_view *doc_view, int active);

void draw_formatted(struct session *ses, int rerender);

void set_frame(struct session *, struct document_view *doc_view, int);
struct document_view *current_frame(struct session *);

void down(struct session *ses, struct document_view *doc_view, int a);
void scroll(struct session *ses, struct document_view *doc_view, int a);

/* Used for changing between formatted and source (plain) view. */
void toggle_plain_html(struct session *ses, struct document_view *doc_view, int a);

/* Used for changing wrapping of text */
void toggle_wrap_text(struct session *ses, struct document_view *doc_view, int a);

/* File menu handlers. */

void save_as(struct session *ses, struct document_view *doc_view, int magic);

/* Various event emitters and link menu handlers. */

void send_event(struct session *, struct term_event *);

void save_formatted_dlg(struct session *ses, struct document_view *doc_view, int a);
void view_image(struct session *ses, struct document_view *doc_view, int a);
void download_link(struct session *ses, struct document_view *doc_view, int image);

void refresh_view(struct session *ses, struct document_view *doc_view, int frames);

#endif
