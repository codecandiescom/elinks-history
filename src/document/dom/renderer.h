/* $Id: renderer.h,v 1.2 2004/09/24 00:44:59 jonas Exp $ */

#ifndef EL__DOCUMENT_DOM_RENDERER_H
#define EL__DOCUMENT_DOM_RENDERER_H

struct cache_entry;
struct document;

void render_dom_document(struct cache_entry *cached, struct document *document);

#endif
