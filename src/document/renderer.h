/* $Id: renderer.h,v 1.3 2003/12/01 14:33:20 pasky Exp $ */

#ifndef EL__DOCUMENT_RENDERER_H
#define EL__DOCUMENT_RENDERER_H

struct document_options;
struct document_view;
struct session;
struct view_state;

void render_document(struct view_state *, struct document_view *, struct document_options *);
void render_document_frames(struct session *ses);

#endif
