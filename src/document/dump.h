/* $Id: dump.h,v 1.3 2002/05/08 13:55:02 pasky Exp $ */

#ifndef EL__DOCUMENT_DUMP_H
#define EL__DOCUMENT_DUMP_C

#include "lowlevel/sched.h"

void dump_end(struct status *, void *);
void dump_start(unsigned char *);

#include "document/html/renderer.h"

int dump_to_file(struct f_data *, int);

#endif
