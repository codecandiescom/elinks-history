/* $Id: dump.h,v 1.2 2002/03/18 06:19:58 pasky Exp $ */

#ifndef EL__DOCUMENT_DUMP_H
#define EL__DOCUMENT_DUMP_C

#include <lowlevel/sched.h>

void dump_end(struct status *, void *);
void dump_start(unsigned char *);

#include <document/html/renderer.h>

int dump_to_file(struct f_data *, int);

#endif
