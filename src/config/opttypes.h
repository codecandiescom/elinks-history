/* $Id: opttypes.h,v 1.6 2002/12/08 16:55:27 pasky Exp $ */

#ifndef EL__CONFIG_OPTTYPES_H
#define EL__CONFIG_OPTTYPES_H

#include "config/options.h"

struct option_type_info {
	unsigned char *name;
	unsigned char *(*cmdline)(struct option *, unsigned char ***, int *);
	unsigned char *(*read)(struct option *, unsigned char **);
	void (*write)(struct option *, unsigned char **, int *);
	/* Return duplicate of option->ptr. */
	void *(*dup)(struct option *, struct option *);
	int (*set)(struct option *, unsigned char *);
	int (*add)(struct option *, unsigned char *);
	int (*remove)(struct option *, unsigned char *);
	unsigned char *help_str;
};

/* enum option_type is index in this array */
extern struct option_type_info option_types[];

extern int commandline;

#endif
