/* $Id: conf.h,v 1.12 2004/01/04 15:41:15 zas Exp $ */

#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "terminal/terminal.h"
#include "util/string.h"

void load_config(void);
void parse_config_file(struct option *options, unsigned char *name,
		       unsigned char *file, struct string *mirror);
int write_config(struct terminal *);

#endif
