/* $Id: draw.h,v 1.1 2004/06/23 08:16:23 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_DRAW_H
#define EL__VIEWER_TEXT_DRAW_H

struct document_view;
struct session;

/* Puts the formatted document on the given terminal's screen. */
void draw_doc(struct session *ses, struct document_view *doc_view, int active);

void draw_formatted(struct session *ses, int rerender);

void refresh_view(struct session *ses, struct document_view *doc_view, int frames);

#endif
