/* $Id: dump.h,v 1.3 2003/07/03 00:28:23 jonas Exp $ */

#ifndef EL__VIEWER_DUMP_DUMP_H
#define EL__VIEWER_DUMP_DUMP_H

#include "sched/connection.h"

void dump_end(struct status *, void *);
void dump_start(unsigned char *);

#include "document/html/renderer.h"

int dump_to_file(struct f_data *, int);

#endif
