/* $Id: conf.h,v 1.5 2002/05/20 11:55:43 pasky Exp $ */

#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "lowlevel/terminal.h"

unsigned char *parse_options(int, unsigned char *[]);
unsigned char *get_token(unsigned char **line);
void load_config();
void write_config(struct terminal *);

#endif
