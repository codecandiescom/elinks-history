/* $Id: urlhist.h,v 1.6 2003/10/04 22:19:23 pasky Exp $ */

#ifndef EL__CONFIG_URLHIST_H
#define EL__CONFIG_URLHIST_H

#include "bfu/inphist.h"

extern int history_dirty;
extern int history_nosave;
extern struct input_history goto_url_history;

int load_url_history(void);
int save_url_history(void);

#endif
