/* $Id: home.h,v 1.5 2005/06/12 00:42:30 jonas Exp $ */

#ifndef EL__CONFIG_HOME_H
#define EL__CONFIG_HOME_H

extern unsigned char *elinks_home;
extern int first_use;

void init_home(void);
void free_home(void);

#endif
