/* $Id: newwin.h,v 1.3 2004/04/15 15:25:39 jonas Exp $ */

#ifndef EL__DIALOG_SYSTEM_H
#define EL__DIALOG_SYSTEM_H

#include "terminal/terminal.h"

struct open_in_new {
	unsigned char *text;
	void (*fn)(struct terminal *term, unsigned char *, unsigned char *);
};

struct open_in_new *get_open_in_new(struct terminal * term);
int can_open_in_new(struct terminal *);

#endif
