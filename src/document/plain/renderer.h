/* $Id: renderer.h,v 1.2 2003/12/01 15:03:30 pasky Exp $ */

#ifndef EL__DOCUMENT_PLAIN_RENDERER_H
#define EL__DOCUMENT_PLAIN_RENDERER_H

struct cache_entry;
struct document;

void render_plain_document(struct cache_entry *ce, struct document *document);

#endif
