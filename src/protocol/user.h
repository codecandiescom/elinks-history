/* $Id: user.h,v 1.5 2003/05/04 17:25:55 pasky Exp $ */

#ifndef EL__MAILTO_H
#define EL__MAILTO_H

#include "terminal/terminal.h"
#include "sched/session.h"

void user_func(struct session *, unsigned char *);

unsigned char *get_prog(struct terminal *, unsigned char *);

#endif
