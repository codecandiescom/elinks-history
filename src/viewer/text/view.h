/* $Id: view.h,v 1.54 2004/06/20 13:13:37 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_VIEW_H
#define EL__VIEWER_TEXT_VIEW_H

struct document_view;
struct session;
struct term_event;
struct terminal;

enum frame_event_status {
	/* The event was not handled */
	FRAME_EVENT_IGNORED,
	/* The event was handled, and the screen should be redrawn */
	FRAME_EVENT_REFRESH,
	/* The event was handled, and the screen should _not_ be redrawn */
	FRAME_EVENT_OK,
};

/* Releases the document view's resources. But doesn't free() the @view. */
void detach_formatted(struct document_view *doc_view);

/* Puts the formatted document on the given terminal's screen. */
void draw_doc(struct session *ses, struct document_view *doc_view, int active);

void draw_formatted(struct session *ses, int rerender);

void set_frame(struct session *ses, struct document_view *doc_view, int xxxx);
struct document_view *current_frame(struct session *);

/* Used for changing between formatted and source (plain) view. */
void toggle_plain_html(struct session *ses, struct document_view *doc_view, int xxxx);

/* Used for changing wrapping of text */
void toggle_wrap_text(struct session *ses, struct document_view *doc_view, int xxxx);

/* File menu handlers. */

void save_as(struct session *ses, struct document_view *doc_view, int magic);

/* Various event emitters and link menu handlers. */

void send_event(struct session *, struct term_event *);

void save_formatted_dlg(struct session *ses, struct document_view *doc_view, int xxxx);
void view_image(struct session *ses, struct document_view *doc_view, int xxxx);
void download_link(struct session *ses, struct document_view *doc_view, int image);

void refresh_view(struct session *ses, struct document_view *doc_view, int frames);

#endif
