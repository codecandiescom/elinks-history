/* $Id: newwin.h,v 1.1 2003/10/27 23:22:11 pasky Exp $ */

#ifndef EL__DIALOG_SYSTEM_H
#define EL__DIALOG_SYSTEM_H

#include "terminal/terminal.h"

struct open_in_new {
	unsigned char *text;
	void (*fn)(struct terminal *term, unsigned char *, unsigned char *);
};

struct open_in_new *get_open_in_new(int);
int can_open_in_new(struct terminal *);

#endif
#endif
