/* $Id: conf.h,v 1.10 2003/05/08 21:43:51 zas Exp $ */

#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "terminal/terminal.h"

void load_config(void);
void parse_config_file(struct option *options, unsigned char *name,
		       unsigned char *file, unsigned char **str, int *len);
void write_config(struct terminal *);

#endif
