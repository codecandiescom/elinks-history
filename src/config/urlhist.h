/* $Id: urlhist.h,v 1.3 2002/12/18 16:25:09 zas Exp $ */

#ifndef EL__CONFIG_URLHIST_H
#define EL__CONFIG_URLHIST_H

int load_url_history();
int save_url_history();
int history_dirty;
int history_nosave;

#endif
