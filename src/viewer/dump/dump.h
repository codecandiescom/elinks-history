/* $Id: dump.h,v 1.2 2003/01/01 20:30:36 pasky Exp $ */

#ifndef EL__VIEWER_DUMP_DUMP_H
#define EL__VIEWER_DUMP_DUMP_H

#include "sched/sched.h"

void dump_end(struct status *, void *);
void dump_start(unsigned char *);

#include "document/html/renderer.h"

int dump_to_file(struct f_data *, int);

#endif
