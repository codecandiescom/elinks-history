/* $Id: conf.h,v 1.3 2002/05/08 13:55:01 pasky Exp $ */

#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "lowlevel/terminal.h"

void init_home();
unsigned char *parse_options(int, unsigned char *[]);
unsigned char *get_token(unsigned char **line);
void load_config();
void write_config(struct terminal *);
void write_html_config(struct terminal *);

#endif
