/* $Id: home.h,v 1.3 2002/05/19 19:34:58 pasky Exp $ */

#ifndef EL__LOWLEVEL_HOME_H
#define EL__LOWLEVEL_HOME_H

extern unsigned char *elinks_home;
extern int first_use;

void init_home();
void free_home();

#endif
