/* $Id: cmdline.h,v 1.3 2004/01/16 20:08:23 jonas Exp $ */

#ifndef EL__CONFIG_CMDLINE_H
#define EL__CONFIG_CMDLINE_H

#include "util/lists.h"

unsigned char *parse_options(int, unsigned char *[], struct list_head *);

#endif
