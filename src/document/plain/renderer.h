/* $Id: renderer.h,v 1.3 2004/04/03 13:17:09 jonas Exp $ */

#ifndef EL__DOCUMENT_PLAIN_RENDERER_H
#define EL__DOCUMENT_PLAIN_RENDERER_H

struct cache_entry;
struct document;

void render_plain_document(struct cache_entry *cache, struct document *document);

#endif
