/* $Id: urlhist.h,v 1.4 2003/05/08 21:46:17 zas Exp $ */

#ifndef EL__CONFIG_URLHIST_H
#define EL__CONFIG_URLHIST_H

int history_dirty;
int history_nosave;

int load_url_history(void);
int save_url_history(void);

#endif
