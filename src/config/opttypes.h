/* $Id: opttypes.h,v 1.2 2002/06/09 14:53:22 pasky Exp $ */

#ifndef EL__CONFIG_OPTTYPES_H
#define EL__CONFIG_OPTTYPES_H

#include "config/options.h"

struct option_type_info {
	unsigned char *(*cmdline)(struct option *, unsigned char ***, int *);
	int (*read)(struct option *, unsigned char **);
	void (*write)(struct option *, unsigned char **, int *);
	void *(*dup)(struct option *); /* Return duplicate of option->ptr. */
	unsigned char *help_str;
};

/* enum option_type is index in this array */
extern struct option_type_info option_types[];

#endif
