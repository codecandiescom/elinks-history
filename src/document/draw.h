/* $Id: draw.h,v 1.2 2003/10/31 20:40:25 jonas Exp $ */

#ifndef EL__DOCUMENT_DRAW_H
#define EL__DOCUMENT_DRAW_H

#include "document/document.h"
#include "terminal/draw.h"
#include "util/color.h"

/* Allocates the requested position in the document and returns the start of
 * the line. */
struct screen_char *
get_document_line(struct document *document, int y, int x, struct color_pair *colors);

#endif
