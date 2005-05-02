/* $Id: cmdline.h,v 1.6 2005/05/02 20:29:06 jonas Exp $ */

#ifndef EL__CONFIG_CMDLINE_H
#define EL__CONFIG_CMDLINE_H

#include "main.h"
#include "util/lists.h"

enum retval parse_options(int, unsigned char *[], struct list_head *);

#endif
