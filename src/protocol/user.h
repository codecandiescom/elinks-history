/* $Id: user.h,v 1.1 2002/08/06 23:26:31 pasky Exp $ */

#ifndef EL__MAILTO_H
#define EL__MAILTO_H

#include "document/session.h"

void mailto_func(struct session *, unsigned char *);
void telnet_func(struct session *, unsigned char *);
void tn3270_func(struct session *, unsigned char *);

#endif
