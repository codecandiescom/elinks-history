/* $Id: mailto.h,v 1.3 2002/05/08 13:55:05 pasky Exp $ */

#ifndef EL__MAILTO_H
#define EL__MAILTO_H

#include "document/session.h"

void mailto_func(struct session *, unsigned char *);
void telnet_func(struct session *, unsigned char *);
void tn3270_func(struct session *, unsigned char *);

#endif
