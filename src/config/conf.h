/* $Id: conf.h,v 1.1 2002/04/27 13:15:51 pasky Exp $ */

#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include <lowlevel/terminal.h>

void init_home();
unsigned char *parse_options(int, unsigned char *[]);
unsigned char *get_token(unsigned char **line);
void load_config();
void write_config(struct terminal *);
void write_html_config(struct terminal *);
void end_config();

int load_url_history();
int save_url_history();

#endif
