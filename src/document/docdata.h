/* $Id: docdata.h,v 1.1 2003/11/18 22:14:08 pasky Exp $ */

#ifndef EL__DOCUMENT_DOCDATA_H
#define EL__DOCUMENT_DOCDATA_H

#include "util/memory.h"

#define LINES_GRANULARITY	0x7F
#define LINE_GRANULARITY	0x0F
#define LINK_GRANULARITY	0x7F

#define ALIGN_LINES(x, o, n) mem_align_alloc(x, o, n, sizeof(struct line), LINES_GRANULARITY)
#define ALIGN_LINE(x, o, n) mem_align_alloc(x, o, n, sizeof(struct screen_char), LINE_GRANULARITY)
#define ALIGN_LINK(x, o, n) mem_align_alloc(x, o, n, sizeof(struct link), LINK_GRANULARITY)

#endif
