/* $Id: conf.h,v 1.4 2002/05/19 16:06:43 pasky Exp $ */

#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "lowlevel/terminal.h"

void init_home();
unsigned char *parse_options(int, unsigned char *[]);
unsigned char *get_token(unsigned char **line);
void load_config();
void write_config(struct terminal *);

#endif
