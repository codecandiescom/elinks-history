/* $Id: dump.h,v 1.10 2004/02/10 06:35:44 witekfl Exp $ */

#ifndef EL__VIEWER_DUMP_DUMP_H
#define EL__VIEWER_DUMP_DUMP_H

struct download;
struct list_head;

void dump_pre_start(struct list_head *);
void dump_end(struct download *, void *);
void dump_start(unsigned char *);

struct string;
struct document;

/* Adds the content of the document to the string line by line. */
struct string *
add_document_to_string(struct string *string, struct document *document);

int dump_to_file(struct document *, int);

#endif
