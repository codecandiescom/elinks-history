/* $Id: dump.h,v 1.6 2003/10/24 16:50:13 jonas Exp $ */

#ifndef EL__VIEWER_DUMP_DUMP_H
#define EL__VIEWER_DUMP_DUMP_H

#include "sched/connection.h"

void dump_end(struct download *, void *);
void dump_start(unsigned char *);

#include "document/html/renderer.h"
#include "util/string.h"

/* Adds the content of the document to the string line by line. */
struct string *
add_document_to_string(struct string *string, struct document *document);

int dump_to_file(struct document *, int);

#endif
