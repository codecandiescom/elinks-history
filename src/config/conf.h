/* $Id: conf.h,v 1.7 2002/08/28 23:27:31 pasky Exp $ */

#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "lowlevel/terminal.h"

void load_config();
void parse_config_file(struct list_head *options, unsigned char *name,
		       unsigned char *file, unsigned char **str, int *len);
void write_config(struct terminal *);

#endif
