#ifndef EL__MAILTO_H
#define EL__MAILTO_H

#include "session.h"

void mailto_func(struct session *, unsigned char *);
void telnet_func(struct session *, unsigned char *);
void tn3270_func(struct session *, unsigned char *);

#endif
