/* $Id: renderer.h,v 1.1 2003/11/10 20:53:21 jonas Exp $ */

#ifndef EL__DOCUMENT_RENDERER_H
#define EL__DOCUMENT_RENDERER_H

#include "document/options.h"
#include "document/view.h"

struct view_state;

void render_document(struct view_state *, struct document_view *, struct document_options *);
void render_document_frames(struct session *ses);

#endif
