/* $Id: draw.h,v 1.1 2003/10/31 19:54:09 jonas Exp $ */

#ifndef EL__DOCUMENT_PLAIN_H
#define EL__DOCUMENT_PLAIN_H

#include "cache/cache.h"
#include "document/document.h"

/* Allocates the requested position in the document and returns the start of
 * the line. */
struct screen_char *get_document_line(struct document *document, int y, int x);

#endif
