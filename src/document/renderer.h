/* $Id: renderer.h,v 1.2 2003/11/18 21:40:46 pasky Exp $ */

#ifndef EL__DOCUMENT_RENDERER_H
#define EL__DOCUMENT_RENDERER_H

#include "document/options.h"
#include "document/view.h"

struct session;
struct view_state;

void render_document(struct view_state *, struct document_view *, struct document_options *);
void render_document_frames(struct session *ses);

#endif
