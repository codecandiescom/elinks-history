/* $Id: conf.h,v 1.6 2002/05/23 18:38:24 pasky Exp $ */

#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "lowlevel/terminal.h"

void load_config();
void write_config(struct terminal *);

#endif
