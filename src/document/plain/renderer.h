/* $Id: renderer.h,v 1.4 2004/04/03 14:13:47 jonas Exp $ */

#ifndef EL__DOCUMENT_PLAIN_RENDERER_H
#define EL__DOCUMENT_PLAIN_RENDERER_H

struct cache_entry;
struct document;

void render_plain_document(struct cache_entry *cached, struct document *document);

#endif
