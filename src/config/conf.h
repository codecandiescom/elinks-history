/* $Id: conf.h,v 1.9 2003/05/04 17:25:52 pasky Exp $ */

#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "terminal/terminal.h"

void load_config();
void parse_config_file(struct option *options, unsigned char *name,
		       unsigned char *file, unsigned char **str, int *len);
void write_config(struct terminal *);

#endif
