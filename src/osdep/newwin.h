/* $Id: newwin.h,v 1.2 2003/10/28 00:00:36 pasky Exp $ */

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
