/* $Id: dump.h,v 1.4 2003/07/04 01:49:03 jonas Exp $ */

#ifndef EL__VIEWER_DUMP_DUMP_H
#define EL__VIEWER_DUMP_DUMP_H

#include "sched/connection.h"

void dump_end(struct download *, void *);
void dump_start(unsigned char *);

#include "document/html/renderer.h"

int dump_to_file(struct f_data *, int);

#endif
