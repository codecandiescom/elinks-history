/* $Id: user.h,v 1.4 2003/01/05 16:48:16 pasky Exp $ */

#ifndef EL__MAILTO_H
#define EL__MAILTO_H

#include "lowlevel/terminal.h"
#include "sched/session.h"

void user_func(struct session *, unsigned char *);

unsigned char *get_prog(struct terminal *, unsigned char *);

#endif
