/* $Id: urlhist.h,v 1.7 2003/10/27 15:33:23 jonas Exp $ */

#ifndef EL__CONFIG_URLHIST_H
#define EL__CONFIG_URLHIST_H

#include "bfu/inphist.h"

extern struct input_history goto_url_history;

void load_url_history(void);
void save_url_history(void);

#endif
