/* $Id: urlhist.h,v 1.5 2003/05/20 10:07:25 zas Exp $ */

#ifndef EL__CONFIG_URLHIST_H
#define EL__CONFIG_URLHIST_H

extern int history_dirty;
extern int history_nosave;
extern struct input_history goto_url_history;

int load_url_history(void);
int save_url_history(void);

#endif
