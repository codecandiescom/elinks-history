/* $Id: dump.h,v 1.5 2003/07/15 12:52:33 jonas Exp $ */

#ifndef EL__VIEWER_DUMP_DUMP_H
#define EL__VIEWER_DUMP_DUMP_H

#include "sched/connection.h"

void dump_end(struct download *, void *);
void dump_start(unsigned char *);

#include "document/html/renderer.h"

int dump_to_file(struct document *, int);

#endif
