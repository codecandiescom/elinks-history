/* $Id: newwin.h,v 1.4 2004/04/15 15:28:52 jonas Exp $ */

#ifndef EL__DIALOG_SYSTEM_H
#define EL__DIALOG_SYSTEM_H

#include "terminal/terminal.h"

struct open_in_new {
	enum term_env_type env;
	void (*fn)(struct terminal *term, unsigned char *, unsigned char *);
	unsigned char *text;
};

struct open_in_new *get_open_in_new(struct terminal * term);
int can_open_in_new(struct terminal *);

#endif
