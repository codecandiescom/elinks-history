/* $Id: view.h,v 1.56 2004/08/15 11:52:52 jonas Exp $ */

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

enum frame_event_status set_frame(struct session *ses, struct document_view *doc_view, int xxxx);
struct document_view *current_frame(struct session *);

/* Used for changing between formatted and source (plain) view. */
void toggle_plain_html(struct session *ses, struct document_view *doc_view, int xxxx);

/* Used for changing wrapping of text */
void toggle_wrap_text(struct session *ses, struct document_view *doc_view, int xxxx);

/* File menu handlers. */

enum frame_event_status save_as(struct session *ses, struct document_view *doc_view, int magic);

/* Various event emitters and link menu handlers. */

void send_event(struct session *, struct term_event *);

enum frame_event_status save_formatted_dlg(struct session *ses, struct document_view *doc_view, int xxxx);
enum frame_event_status view_image(struct session *ses, struct document_view *doc_view, int xxxx);
enum frame_event_status download_link(struct session *ses, struct document_view *doc_view, int action);

#endif
