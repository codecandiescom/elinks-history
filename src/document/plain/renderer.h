/* $Id: renderer.h,v 1.1 2003/11/10 21:29:30 jonas Exp $ */

#ifndef EL__DOCUMENT_PLAIN_RENDERER_H
#define EL__DOCUMENT_PLAIN_RENDERER_H

#include "cache/cache.h"
#include "document/document.h"

void render_plain_document(struct cache_entry *ce, struct document *document);

#endif
