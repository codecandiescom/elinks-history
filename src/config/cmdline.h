/* $Id: cmdline.h,v 1.2 2004/01/16 18:21:05 zas Exp $ */

#ifndef EL__CONFIG_CMDLINE_H
#define EL__CONFIG_CMDLINE_H

#include "util/lists.h"

struct list_head *parse_options(int, unsigned char *[], int);

#endif
