/* $Id: user.h,v 1.3 2002/12/01 17:45:11 pasky Exp $ */

#ifndef EL__MAILTO_H
#define EL__MAILTO_H

#include "document/session.h"
#include "lowlevel/terminal.h"

void user_func(struct session *, unsigned char *);

unsigned char *get_prog(struct terminal *, unsigned char *);

#endif
