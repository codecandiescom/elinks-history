/* $Id: conf.h,v 1.11 2003/07/21 05:47:43 jonas Exp $ */

#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "terminal/terminal.h"
#include "util/string.h"

void load_config(void);
void parse_config_file(struct option *options, unsigned char *name,
		       unsigned char *file, struct string *mirror);
void write_config(struct terminal *);

#endif
