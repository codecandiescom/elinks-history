/* $Id: dump.h,v 1.12 2004/05/16 12:11:51 jonas Exp $ */

#ifndef EL__VIEWER_DUMP_DUMP_H
#define EL__VIEWER_DUMP_DUMP_H

#include "util/lists.h"

struct download;
struct string;
struct document;

/* Adds the content of the document to the string line by line. */
struct string *
add_document_to_string(struct string *string, struct document *document);

int dump_to_file(struct document *, int);
void dump_pre_start(struct list_head *);

#endif
