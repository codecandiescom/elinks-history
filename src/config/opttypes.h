/* $Id: opttypes.h,v 1.1 2002/05/23 18:50:36 pasky Exp $ */

#ifndef EL__CONFIG_OPTTYPES_H
#define EL__CONFIG_OPTTYPES_H

#include "config/options.h"

struct option_type_info {
	unsigned char *(*cmdline)(struct option *, unsigned char ***, int *);
	int (*read)(struct option *, unsigned char **);
	void (*write)(struct option *, unsigned char **, int *);
	unsigned char *help_str;
};

/* enum option_type is index in this array */
extern struct option_type_info option_types[];

#endif
