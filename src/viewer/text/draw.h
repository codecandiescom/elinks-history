/* $Id: draw.h,v 1.2 2004/07/13 14:48:58 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_DRAW_H
#define EL__VIEWER_TEXT_DRAW_H

struct document_view;
struct session;

void draw_formatted(struct session *ses, int rerender);

void refresh_view(struct session *ses, struct document_view *doc_view, int frames);

#endif
